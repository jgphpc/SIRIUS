#include "band.h"

namespace sirius {

int Band::residuals(K_point* kp__,
                    int N__,
                    int num_bands__,
                    std::vector<double>& eval__,
                    std::vector<double>& eval_old__,
                    matrix<double_complex>& evec__,
                    Wave_functions& hphi__,
                    Wave_functions& ophi__,
                    Wave_functions& hpsi__,
                    Wave_functions& opsi__,
                    Wave_functions& res__,
                    std::vector<double>& h_diag__,
                    std::vector<double>& o_diag__)
{
    PROFILE_WITH_TIMER("sirius::Band::residuals");

    auto& itso = kp__->iterative_solver_input_section_;
    bool converge_by_energy = (itso.converge_by_energy_ == 1);

    /* norm of residuals */
    std::vector<double> res_norm(num_bands__);

    int n = 0;
    if (converge_by_energy)
    {
        std::vector<double> eval_tmp(num_bands__);

        /* main trick here: first estimate energy difference, and only then compute unconverged residuals */
        double tol = ctx_.iterative_solver_tolerance();
        for (int i = 0; i < num_bands__; i++)
        {
            if (kp__->band_occupancy(i) > 1e-10 && std::abs(eval__[i] - eval_old__[i]) > tol)
            {
                std::memcpy(&evec__(0, num_bands__ + n), &evec__(0, i), N__ * sizeof(double_complex));
                eval_tmp[n++] = eval__[i];
            }
        }
        //TODO: do this on GPU

        /* create alias for eigen-vectors corresponding to unconverged residuals */
        matrix<double_complex> evec_tmp;
        if (parameters_.processing_unit() == CPU)
        {
            evec_tmp = matrix<double_complex>(&evec__(0, num_bands__), evec__.ld(), n);
        }
        #ifdef __GPU
        if (parameters_.processing_unit() == GPU)
        {
            evec_tmp = matrix<double_complex>(evec__.at<CPU>(0, num_bands__), evec__.at<GPU>(0, num_bands__), evec__.ld(), n);
            /* move matrix of eigen-vectors to GPU */
            acc::copyin(evec_tmp.at<GPU>(), evec_tmp.ld(), evec_tmp.at<CPU>(), evec_tmp.ld(), N__, n);
        }
        #endif

        /* compute H\Psi_{i} = \sum_{mu} H\phi_{mu} * Z_{mu, i} */
        hpsi__.transform_from(hphi__, N__, evec_tmp, n);
        /* compute O\Psi_{i} = \sum_{mu} O\phi_{mu} * Z_{mu, i} */
        opsi__.transform_from(ophi__, N__, evec_tmp, n);

        residuals_aux(kp__, n, eval_tmp, hpsi__, opsi__, res__, h_diag__, o_diag__, res_norm);
    }
    else
    {
        /* compute H\Psi_{i} = \sum_{mu} H\phi_{mu} * Z_{mu, i} */
        hpsi__.transform_from(hphi__, N__, evec__, num_bands__);
        /* compute O\Psi_{i} = \sum_{mu} O\phi_{mu} * Z_{mu, i} */
        opsi__.transform_from(ophi__, N__, evec__, num_bands__);

        residuals_aux(kp__, num_bands__, eval__, hpsi__, opsi__, res__, h_diag__, o_diag__, res_norm);

        for (int i = 0; i < num_bands__; i++)
        {
            /* take the residual if it's norm is above the threshold */
            if (res_norm[i] > ctx_.iterative_solver_tolerance() && kp__->band_occupancy(i) > 1e-10)
            {
                /* shift unconverged residuals to the beginning of array */
                if (n != i)
                {
                    switch (parameters_.processing_unit())
                    {
                        case CPU:
                        {
                            std::memcpy(&res__(0, n), &res__(0, i), res__.num_gvec_loc() * sizeof(double_complex));
                            break;
                        }
                        //case GPU:
                        //{
                        //    #ifdef __GPU
                        //    cuda_copy_device_to_device(res_tmp.at<GPU>(0, n), res_tmp.at<GPU>(0, i), ngk * sizeof(double_complex));
                        //    #else
                        //    TERMINATE_NO_GPU
                        //    #endif
                        //    break;
                        //}
                    }
                }
                n++;
            }
        }
        //#ifdef __GPU
        //if (parameters_.processing_unit() == GPU && economize_gpu_memory)
        //{
        //    /* copy residuals to CPU because the content of kappa array will be destroyed */
        //    cublas_get_matrix(ngk, n, sizeof(double_complex), res_tmp.at<GPU>(), res_tmp.ld(),
        //                      res.at<CPU>(), res.ld());
        //}
        //#endif
    }

    return n;
}

};
