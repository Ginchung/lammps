/*
 * Copyright (C) 2022 AdvanceSoft Corporation
 *
 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 */

#include "dump_nnp.h"

#include "atom.h"
#include "compute.h"
#include "domain.h"
#include "error.h"
#include "force.h"
#include "memory.h"
#include "modify.h"
#include "update.h"

using namespace LAMMPS_NS;

#define ONELINE 128
#define DELTA 1048576

#define COEF_LENGTH 1.88972612
#define COEF_ENERGY 6.33363068e-6
#define COEF_FORCE 3.35161302e-6

#define FOR_SANNP 0

DumpNNP::DumpNNP(LAMMPS *lmp, int narg, char **arg) : Dump(lmp, narg, arg)
{
    size_one = 13;

    x2ryd = COEF_LENGTH / force->angstrom;
    e2ryd = COEF_ENERGY / force->boltz;
    f2ryd = COEF_FORCE * force->angstrom / force->boltz;
    q2ryd = 1.0 / force->qelectron;

    nevery = utils::inumeric(FLERR, arg[3], false, lmp);
    if (nevery <= 0) error->all(FLERR, "Illegal dump custom command");

    delete[] format_default;

    format_default = utils::strdup("%5d%20.12E%20.12E%20.12E%20.12E%20.12E%20.12E%20.12E%20.12E%20.12E%20.12E%20.12E%20.12E");

    pe = nullptr;
    peatom = modify->add_compute("dump_nnp_peatom all pe/atom");
}

DumpNNP::~DumpNNP()
{
    delete[] format_default;
    format_default = nullptr;

    modify->delete_compute("dump_nnp_peatom");
}

void DumpNNP::init_style()
{
    format = utils::strdup(fmt::format("{}\n", format_default));

    pe = modify->get_compute_by_id("thermo_pe");

    if (multifile == 0) openfile();
}

void DumpNNP::write_header(bigint n)
{
    if (me == 0)
    {
        std::string header = fmt::format("{:8}{:8}    {:8}\n", n, FOR_SANNP, pe->scalar);

        xdim = (domain->boxhi[0] - domain->boxlo[0]) * x2ryd;
        ydim = (domain->boxhi[1] - domain->boxlo[1]) * x2ryd;
        zdim = (domain->boxhi[2] - domain->boxlo[2]) * x2ryd;

        if (domain->triclinic)
        {
            xy = domain->xy * x2ryd;
            xz = domain->xz * x2ryd;
            yz = domain->yz * x2ryd;

            header += fmt::format("{:20.12E}{:20.12E}{:20.12E}\n", xdim, 0.0,  0.0 );
            header += fmt::format("{:20.12E}{:20.12E}{:20.12E}\n", xy,   ydim, 0.0 );
            header += fmt::format("{:20.12E}{:20.12E}{:20.12E}\n", xz,   yz,   zdim);
        }
        else
        {
            header += fmt::format("{:20.12E}{:20.12E}{:20.12E}\n", xdim, 0.0,  0.0 );
            header += fmt::format("{:20.12E}{:20.12E}{:20.12E}\n", 0.0,  ydim, 0.0 );
            header += fmt::format("{:20.12E}{:20.12E}{:20.12E}\n", 0.0,  0.0,  zdim);
        }

        fmt::print(fp, header);
    }
}

int DumpNNP::count()
{
    if (update->whichflag == 0)
    {
        if (pe->invoked_peratom != update->ntimestep)
            error->all(FLERR, "Compute used in dump between runs is not current");

        if (peatom->invoked_peratom != update->ntimestep)
            error->all(FLERR, "Compute used in dump between runs is not current");
    }
    else
    {
        if (!(pe->invoked_flag & Compute::INVOKED_SCALAR))
        {
            pe->compute_scalar();
        }

        if(!(peatom->invoked_flag & Compute::INVOKED_PERATOM))
        {
            peatom->compute_peratom();
        }
    }

    pe->addstep(update->ntimestep + nevery);
    peatom->addstep(update->ntimestep + nevery);

    return Dump::count();
}

void DumpNNP::pack(tagint */*ids*/)
{
    int m;

    int *type = atom->type;
    int *mask = atom->mask;
    double **x = atom->x;
    double **f = atom->f;
    double *q = atom->q;
    int nlocal = atom->nlocal;

    double *eatom = peatom->vector_atom;

    m = 0;
    for (int i = 0; i < nlocal; i++)
    {
        if (mask[i] & groupbit)
        {
            buf[m++] = type[i];
            buf[m++] = x[i][0] * x2ryd;
            buf[m++] = x[i][1] * x2ryd;
            buf[m++] = x[i][2] * x2ryd;
            buf[m++] = eatom ? eatom[i] * e2ryd : 0.0;
            buf[m++] = f[i][0] * f2ryd;
            buf[m++] = f[i][1] * f2ryd;
            buf[m++] = f[i][2] * f2ryd;
            buf[m++] = atom->q_flag ? q[i] * q2ryd : 0.0;
            buf[m++] = 0.0; // coulomb energy
            buf[m++] = 0.0; // coulomb force x
            buf[m++] = 0.0; // coulomb force y
            buf[m++] = 0.0; // coulomb force z
        }
    }
}

int DumpNNP::convert_string(int n, double *mybuf)
{
    int offset = 0;
    int m = 0;

    for (int i = 0; i < n; i++)
    {
        if (offset + ONELINE > maxsbuf) 
        {
            if ((bigint) maxsbuf + DELTA > MAXSMALLINT) return -1;
            maxsbuf += DELTA;
            memory->grow(sbuf, maxsbuf, "dump:sbuf");
        }

        offset += sprintf(&sbuf[offset], format,
                          static_cast<int> (mybuf[m     ]), // TODO: map to element name
                          mybuf[m +  1], mybuf[m +  2], mybuf[m +  3],
                          mybuf[m +  4],
                          mybuf[m +  5], mybuf[m +  6], mybuf[m +  7],
                          mybuf[m +  8], mybuf[m +  9],
                          mybuf[m + 10], mybuf[m + 11], mybuf[m + 12]);
        
        offset += sprintf(&sbuf[offset],"\n");

        m += size_one;
    }

    return offset;
}

void DumpNNP::write_data(int n, double *mybuf)
{
    if (buffer_flag == 1)
    {
        if (mybuf) fwrite(mybuf, sizeof(char), n, fp);
    }
    else
    {
        int m = 0;

        for (int i = 0; i < n; i++)
        {
            fprintf(fp, format,
                    static_cast<int> (mybuf[m     ]),
                    mybuf[m +  1], mybuf[m +  2], mybuf[m +  3],
                    mybuf[m +  4],
                    mybuf[m +  5], mybuf[m +  6], mybuf[m +  7],
                    mybuf[m +  8], mybuf[m +  9],
                    mybuf[m + 10], mybuf[m + 11], mybuf[m + 12]);

            m += size_one;
        }
    }
}