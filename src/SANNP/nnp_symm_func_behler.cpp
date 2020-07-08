/*
 * Copyright (C) 2020 AdvanceSoft Corporation
 *
 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 */

#include "nnp_symm_func_behler.h"

SymmFuncBehler::SymmFuncBehler(int numElems, int sizeRad, int sizeAng, real radiusCut,
                               const real* radiusEta, const real* radiusShift,
                               const real* angleEta,  const real* angleZeta) : SymmFunc(numElems)
{
    if (sizeRad < 1)
    {
        stop_by_error("size of radius basis is not positive.");
    }

    if (sizeAng < 0)
    {
        stop_by_error("size of angle basis is negative.");
    }

    if (radiusCut <= ZERO)
    {
        stop_by_error("cutoff radius is not positive.");
    }

    if (radiusEta == NULL)
    {
        stop_by_error("radiusEta is null.");
    }

    if (radiusShift == NULL)
    {
        stop_by_error("radiusShift is null.");
    }

    if (sizeAng > 0 && angleEta == NULL)
    {
        stop_by_error("angleEta is null.");
    }

    if (sizeAng > 0 && angleZeta == NULL)
    {
        stop_by_error("angleZeta is null.");
    }

    this->sizeRad = sizeRad;
    this->sizeAng = sizeAng;

    this->numRadBasis = this->sizeRad * this->numElems;
    this->numAngBasis = this->sizeAng * 2 * (this->numElems * (this->numElems + 1) / 2);

    this->numBasis = this->numRadBasis + this->numAngBasis;

    this->radiusCut = radiusCut;

    this->radiusEta   = radiusEta;
    this->radiusShift = radiusShift;

    this->angleEta  = angleEta;
    this->angleZeta = angleZeta;
}

SymmFuncBehler::~SymmFuncBehler()
{
    // NOP
}

void SymmFuncBehler::calculate(int numNeighbor, real** posNeighbor, int* elemNeighbor,
                               real* symmData, real* symmDiff) const
{
    if (posNeighbor == NULL || elemNeighbor == NULL)
    {
        stop_by_error("neighbor is null.");
    }

    if (symmData == NULL)
    {
        stop_by_error("symmData is null.");
    }

    if (symmDiff == NULL)
    {
        stop_by_error("symmDiff is null.");
    }

    // define varialbes
    const int numFree = 3 * (1 + numNeighbor);

    int ineigh1, ineigh2;

    int jelem1, jelem2;
    int ifree1, ifree2;

    int imode;
    int ibase, jbase, kbase;

    real x1, x2;
    real y1, y2;
    real z1, z2;
    real r1, r2, dr, rr;

    real  rs, eta;
    real  zeta, zeta0;
    real* zeta1;

    int  ilambda;
    real lambda;

    real tanh1;
    real tanh2;

    real fc1, fc2;
    real dfc1dr1, dfc2dr2;
    real dfc1dx1, dfc2dx2;
    real dfc1dy1, dfc2dy2;
    real dfc1dz1, dfc2dz2;

    real fc12;
    real dfc12dx1, dfc12dx2;
    real dfc12dy1, dfc12dy2;
    real dfc12dz1, dfc12dz2;

    real gau;
    real dgaudx1, dgaudx2;
    real dgaudy1, dgaudy2;
    real dgaudz1, dgaudz2;

    real psi;
    real dpsidx1, dpsidx2;
    real dpsidy1, dpsidy2;
    real dpsidz1, dpsidz2;

    real chi;
    real chi0;
    real dchidpsi;
    const real chi0_thr = REAL(1.0e-6);

    real g;
    real dgdx1, dgdx2;
    real dgdy1, dgdy2;
    real dgdz1, dgdz2;

    real coef0, coef1, coef2;

    // initialize symmetry functions
    for (ibase = 0; ibase < this->numBasis; ++ibase)
    {
        symmData[ibase] = ZERO;
    }

    for (ifree1 = 0; ifree1 < numFree; ++ifree1)
    {
        for (ibase = 0; ibase < this->numBasis; ++ibase)
        {
            symmDiff[ibase + ifree1 * this->numBasis] = ZERO;
        }
    }

    if (numNeighbor < 1)
    {
        return;
    }

    // radial part
    for (ineigh1 = 0; ineigh1 < numNeighbor; ++ineigh1)
    {
        ifree1 = 3 * (ineigh1 + 1);
        jelem1 = elemNeighbor[ineigh1];

        jbase = jelem1 * this->sizeRad;

        r1 = posNeighbor[ineigh1][0];
        x1 = posNeighbor[ineigh1][1];
        y1 = posNeighbor[ineigh1][2];
        z1 = posNeighbor[ineigh1][3];

        tanh1   = tanh(ONE - r1 / this->radiusCut);
        tanh2   = tanh1 * tanh1;
        fc1     = tanh1 * tanh2;
        dfc1dr1 = -REAL(3.0) * tanh2 * (ONE - tanh2) / this->radiusCut;
        dfc1dx1 = x1 / r1 * dfc1dr1;
        dfc1dy1 = y1 / r1 * dfc1dr1;
        dfc1dz1 = z1 / r1 * dfc1dr1;

        for (imode = 0; imode < this->sizeRad; ++imode)
        {
            eta = this->radiusEta[imode];
            rs  = this->radiusShift[imode];

            dr  = r1 - rs;
            rr  = dr * dr;

            gau     = exp(-eta * rr);
            coef0   = -REAL(2.0) * eta * dr / r1 * gau;
            dgaudx1 = x1 * coef0;
            dgaudy1 = y1 * coef0;
            dgaudz1 = z1 * coef0;

            g     = gau * fc1;
            dgdx1 = dgaudx1 * fc1 + gau * dfc1dx1;
            dgdy1 = dgaudy1 * fc1 + gau * dfc1dy1;
            dgdz1 = dgaudz1 * fc1 + gau * dfc1dz1;

            ibase = imode + jbase;

            symmData[ibase] += g;

            symmDiff[ibase + 0 * this->numBasis] -= dgdx1;
            symmDiff[ibase + 1 * this->numBasis] -= dgdy1;
            symmDiff[ibase + 2 * this->numBasis] -= dgdz1;

            symmDiff[ibase + (ifree1 + 0) * this->numBasis] += dgdx1;
            symmDiff[ibase + (ifree1 + 1) * this->numBasis] += dgdy1;
            symmDiff[ibase + (ifree1 + 2) * this->numBasis] += dgdz1;
        }
    }

    if (numNeighbor < 2 || this->sizeAng < 1)
    {
        return;
    }

    // angular part
    zeta1 = new real[this->sizeAng];
    for (imode = 0; imode < this->sizeAng; ++imode)
    {
        zeta = this->angleZeta[imode];
        zeta1[imode] = pow(REAL(2.0), ONE - zeta);
    }

    for (ineigh2 = 0; ineigh2 < numNeighbor; ++ineigh2)
    {
        ifree2 = 3 * (ineigh2 + 1);
        jelem2 = elemNeighbor[ineigh2];

        r2 = posNeighbor[ineigh2][0];
        x2 = posNeighbor[ineigh2][1];
        y2 = posNeighbor[ineigh2][2];
        z2 = posNeighbor[ineigh2][3];

        tanh1   = tanh(ONE - r2 / this->radiusCut);
        tanh2   = tanh1 * tanh1;
        fc2     = tanh1 * tanh2;
        dfc2dr2 = -REAL(3.0) * tanh2 * (ONE - tanh2) / this->radiusCut;
        dfc2dx2 = x2 / r2 * dfc2dr2;
        dfc2dy2 = y2 / r2 * dfc2dr2;
        dfc2dz2 = z2 / r2 * dfc2dr2;

        for (ineigh1 = 0; ineigh1 < numNeighbor; ++ineigh1)
        {
            ifree1 = 3 * (ineigh1 + 1);
            jelem1 = elemNeighbor[ineigh1];

            if (jelem1 > jelem2 || (jelem1 == jelem2 && ineigh1 >= ineigh2))
            {
                continue;
            }

            kbase = (jelem1 + jelem2 * (jelem2 + 1) / 2) * 2 * this->sizeAng;

            r1 = posNeighbor[ineigh1][0];
            x1 = posNeighbor[ineigh1][1];
            y1 = posNeighbor[ineigh1][2];
            z1 = posNeighbor[ineigh1][3];

            rr = r1 * r1 + r2 * r2;

            tanh1   = tanh(ONE - r1 / this->radiusCut);
            tanh2   = tanh1 * tanh1;
            fc1     = tanh1 * tanh2;
            dfc1dr1 = -REAL(3.0) * tanh2 * (ONE - tanh2) / this->radiusCut;
            dfc1dx1 = x1 / r1 * dfc1dr1;
            dfc1dy1 = y1 / r1 * dfc1dr1;
            dfc1dz1 = z1 / r1 * dfc1dr1;

            fc12 = fc1 * fc2;
            dfc12dx1 = dfc1dx1 * fc2;
            dfc12dy1 = dfc1dy1 * fc2;
            dfc12dz1 = dfc1dz1 * fc2;
            dfc12dx2 = fc1 * dfc2dx2;
            dfc12dy2 = fc1 * dfc2dy2;
            dfc12dz2 = fc1 * dfc2dz2;

            psi     = (x1 * x2 + y1 * y2 + z1 * z2) / r1 / r2;
            coef0   = ONE / r1 / r2;
            coef1   = psi / r1 / r1;
            coef2   = psi / r2 / r2;
            dpsidx1 = coef0 * x2 - coef1 * x1;
            dpsidy1 = coef0 * y2 - coef1 * y1;
            dpsidz1 = coef0 * z2 - coef1 * z1;
            dpsidx2 = coef0 * x1 - coef2 * x2;
            dpsidy2 = coef0 * y1 - coef2 * y2;
            dpsidz2 = coef0 * z1 - coef2 * z2;

            for (ilambda = 0; ilambda < 2; ++ilambda)
            {
                lambda = (ilambda == 0) ? ONE : (-ONE);

                chi0 = ONE + lambda * psi;
                if (chi0 < chi0_thr)
                {
                    continue;
                }

                jbase = ilambda * this->sizeAng;

                for (imode = 0; imode < this->sizeAng; ++imode)
                {
                    eta   = this->angleEta[imode];
                    zeta  = this->angleZeta[imode];
                    zeta0 = zeta1[imode];

                    chi      = zeta0 * pow(chi0, zeta);
                    dchidpsi = zeta * lambda * chi / chi0;

                    gau     = exp(-eta * rr);
                    coef0   = -REAL(2.0) * eta * gau;
                    dgaudx1 = x1 * coef0;
                    dgaudy1 = y1 * coef0;
                    dgaudz1 = z1 * coef0;
                    dgaudx2 = x2 * coef0;
                    dgaudy2 = y2 * coef0;
                    dgaudz2 = z2 * coef0;

                    g     = chi * gau * fc12;
                    dgdx1 = dchidpsi * dpsidx1 * gau * fc12 + chi * dgaudx1 * fc12 + chi * gau * dfc12dx1;
                    dgdy1 = dchidpsi * dpsidy1 * gau * fc12 + chi * dgaudy1 * fc12 + chi * gau * dfc12dy1;
                    dgdz1 = dchidpsi * dpsidz1 * gau * fc12 + chi * dgaudz1 * fc12 + chi * gau * dfc12dz1;
                    dgdx2 = dchidpsi * dpsidx2 * gau * fc12 + chi * dgaudx2 * fc12 + chi * gau * dfc12dx2;
                    dgdy2 = dchidpsi * dpsidy2 * gau * fc12 + chi * dgaudy2 * fc12 + chi * gau * dfc12dy2;
                    dgdz2 = dchidpsi * dpsidz2 * gau * fc12 + chi * dgaudz2 * fc12 + chi * gau * dfc12dz2;

                    ibase = this->numRadBasis + imode + jbase + kbase;

                    symmData[ibase] += g;

                    symmDiff[ibase + 0 * this->numBasis] -= dgdx1 + dgdx2;
                    symmDiff[ibase + 1 * this->numBasis] -= dgdy1 + dgdy2;
                    symmDiff[ibase + 2 * this->numBasis] -= dgdz1 + dgdz2;

                    symmDiff[ibase + (ifree1 + 0) * this->numBasis] += dgdx1;
                    symmDiff[ibase + (ifree1 + 1) * this->numBasis] += dgdy1;
                    symmDiff[ibase + (ifree1 + 2) * this->numBasis] += dgdz1;

                    symmDiff[ibase + (ifree2 + 0) * this->numBasis] += dgdx2;
                    symmDiff[ibase + (ifree2 + 1) * this->numBasis] += dgdy2;
                    symmDiff[ibase + (ifree2 + 2) * this->numBasis] += dgdz2;
                }
            }
        }
    }

    delete[] zeta1;
}