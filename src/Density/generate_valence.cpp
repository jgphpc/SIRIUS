#include "density.h"

namespace sirius {

void Density::generate_valence(K_set& ks__)
{
    Timer t("sirius::Density::generate_valence", ctx_.comm());
    
    double wt = 0.0;
    double ot = 0.0;
    for (int ik = 0; ik < ks__.num_kpoints(); ik++)
    {
        wt += ks__[ik]->weight();
        for (int j = 0; j < parameters_.num_bands(); j++) ot += ks__[ik]->weight() * ks__[ik]->band_occupancy(j);
    }

    if (std::abs(wt - 1.0) > 1e-12) error_local(__FILE__, __LINE__, "K_point weights don't sum to one");

    if (std::abs(ot - unit_cell_.num_valence_electrons()) > 1e-8)
    {
        std::stringstream s;
        s << "wrong occupancies" << std::endl
          << "  computed : " << ot << std::endl
          << "  required : " << unit_cell_.num_valence_electrons() << std::endl
          << "  difference : " << fabs(ot - unit_cell_.num_valence_electrons());
        warning_local(__FILE__, __LINE__, s);
    }
    
    if (parameters_.esm_type() == ultrasoft_pseudopotential)
    {
        for (int ikloc = 0; ikloc < (int)ks__.spl_num_kpoints().local_size(); ikloc++)
        {
            int ik = ks__.spl_num_kpoints(ikloc);
            auto kp = ks__[ik];
            auto occupied_bands = kp->get_occupied_bands_list();
            
            if (kp->num_ranks() > 1)
            {
                linalg<CPU>::gemr2d(kp->wf_size(), occupied_bands.num_occupied_bands(),
                                    kp->fv_states(), 0, 0,
                                    kp->fv_states_slice(), 0, 0,
                                    kp->blacs_grid().context());
            }
        }
    }

    /* zero density and magnetization */
    zero();

    /* interstitial part is independent of basis type */
    generate_valence_density_it(ks__);

    /* for muffin-tin part */
    switch (parameters_.esm_type())
    {
        case full_potential_lapwlo:
        {
            generate_valence_density_mt(ks__);
            break;
        }
        case full_potential_pwlo:
        {
            STOP();
        }
        default:
        {
            break;
        }
    }

    for (int ir = 0; ir < fft_->size(); ir++)
    {
        if (rho_->f_it<global>(ir) < 0) TERMINATE("density is wrong");
    }
    
    /* get rho(G) */
    rho_->fft_transform(-1);

    if (parameters_.esm_type() == ultrasoft_pseudopotential) augment(ks__);
}

};
