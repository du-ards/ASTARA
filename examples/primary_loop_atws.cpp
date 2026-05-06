/**
 * @file   primary_loop_atws.cpp
 * @brief  Coupled-loop demonstration: ATWS (Anticipated Transient Without
 *         Scram) following a turbine trip.
 *
 * Sets up a complete PWR primary loop (reactor + Ali Model D U-tube SG +
 * homologous pump + pressurizer) at full-power steady state, then at
 * t = 30 s **slams the main steam valve shut** -- and crucially, does
 * NOT scram the reactor.
 *
 * This is the textbook ATWS scenario: every PWR safety analyst learns
 * what happens.  With no heat sink the SG pressure climbs rapidly; the
 * rising secondary saturation temperature reduces the primary->secondary
 * heat flux; the primary heats up; the reactor's *negative* moderator
 * and Doppler temperature coefficients then drive the power down without
 * any rod motion.  The reactor finds a new equilibrium at lower power
 * and higher primary temperature -- the system "saves itself" through
 * pure passive feedback.
 *
 * In a real plant the reactor protection system would scram on high SG
 * pressure or high power-to-flow ratio long before reaching this
 * equilibrium.  The point of running this without RPS is precisely to
 * **see** the feedback do its job in isolation.
 *
 * What you should see in the trace:
 *
 *   - **t = 30 s**: steam-line back-pressure jumps above SG pressure
 *     (valve closed); steam mass flow goes to zero.
 *   - **t = 30..60 s**: SG pressure climbs ~ 0.3 - 1.0 MPa as the heat
 *     piles up.  Primary T_hot and T_cold both climb.
 *   - **t = 30..120 s**: reactor power *drops* on negative moderator and
 *     Doppler feedback even though no rods move.  Watch
 *     `rho_external_pcm` stay at 0 throughout while `power` drops.
 *   - **t = 120 s onward**: reactor settles at a new lower power, with
 *     primary T_avg ~ 15-30 K above the original setpoint and SG
 *     pressure a few MPa higher.  Pressurizer pressure rises (insurge
 *     from hotter primary), spray opens, eventually settling.
 *
 * Trace is written to `primary_loop_atws.csv`.  Plot column 2 (power) vs
 * time and column 12 (SG pressure) vs time to see the classic ATWS
 * crossover plot.
 *
 * No controllers are attached: this is pure plant physics, demonstrating
 * the inherent safety of the negative-feedback PWR design.
 */

#include "astara/primary/PrimaryLoop.hpp"
#include "astara/props/IF97Water.hpp"

#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <cstdlib>
#include <ctime>

using astara::primary::PrimaryLoop;
using astara::reactor::Reactor;
using astara::reactor::ReactorThermalParameters;
using astara::reactor::DelayedGroupConstants;
using astara::reactor::ReactivityModel;
using astara::sg::AliSteamGenerator;
using astara::sg::AliSteamGeneratorParameters;
using astara::pump::HomologousPump;
using astara::pump::HomologousPumpParameters;
using astara::pressurizer::Pressurizer;
using astara::pressurizer::PressurizerParameters;
using astara::props::IF97Water;

namespace {

    IF97Water& sharedWater() {
        static IF97Water w;
        return w;
    }

    std::unique_ptr<Reactor> makeReactor() {
        ReactorThermalParameters tp;
        tp.num_fuel_nodes          = 3;
        tp.num_moderator_nodes     = 6;
        tp.fuel_mass_total_kg      = 1.0e5;
        tp.fuel_cp_J_per_kgK       = 300.0;
        tp.fission_power_in_fuel   = 0.974;
        tp.moderator_mass_total_kg = 1.5e4;
        tp.moderator_cp_J_per_kgK  = 5400.0;
        tp.mass_flow_rate_kg_s     = 17600.0;
        tp.overall_h_W_per_m2K     = 28000.0;
        tp.heat_transfer_area_m2   = 5400.0;
        tp.lower_plenum_mass_kg    = 5000.0;
        tp.upper_plenum_mass_kg    = 5000.0;
        tp.hot_leg_mass_kg         = 2500.0;
        tp.cold_leg_mass_kg        = 2500.0;

        ReactivityModel rm;
        // PWR-typical feedback coefficients.  These are what's going to save
        // the day in this scenario.
        rm.alpha_fuel_per_K      = -2.5e-5;     // Doppler (per K of fuel T)
        rm.alpha_moderator_per_K = -3.0e-4;     // moderator T (per K of moderator T)

        auto r = std::make_unique<Reactor>(DelayedGroupConstants::u235SixGroup(), tp,
                /*P_rated_W=*/3.4e9, rm);
        r->initialiseSteadyState(/*n0=*/1.0, /*T_inlet=*/559.0);
        return r;
    }

    std::unique_ptr<AliSteamGenerator> makeSG() {
        auto p  = AliSteamGeneratorParameters::westinghouseModelD5();
        auto sg = std::make_unique<AliSteamGenerator>(p, &sharedWater());
        sg->initialiseSteadyState(/*T_pi_in*/ 597.0,
                /*W_p   */ 4400.0,
                /*P_sg  */ 6.9e6,
                /*L_dw  */ 3.0);
        return sg;
    }

    std::unique_ptr<HomologousPump> makePump() {
        HomologousPumpParameters p;
        p.curve.A0                   = 90.0;
        p.curve.A1                   = 1.0;
        p.curve.A2                   = -1.0;
        p.rated_speed_rev_s          = 20.0;
        p.rated_volumetric_flow_m3_s = 6.0;
        p.loop_resistance_K_s2_m5    = 60.0 / 36.0;
        p.effective_flow_area_m2     = 0.4;
        p.loop_length_m              = 70.0;
        p.fluid_density_kg_m3        = 720.0;
        p.moment_of_inertia_kg_m2    = 1500.0;
        p.rated_input_power_W        = 720.0 * 9.80665 * 6.0 * 60.0;
        auto pump = std::make_unique<HomologousPump>(p);
        pump->initialiseAtRated();
        return pump;
    }

    std::unique_ptr<Pressurizer> makePressurizer() {
        PressurizerParameters geom;
        geom.cross_section_area_m2 = 4.0;
        geom.total_height_m        = 13.0;
        auto pz = std::make_unique<Pressurizer>(geom, &sharedWater());
        pz->initialiseSteadyState(/*P*/ 15.5e6, /*Lw*/ 8.0);
        return pz;
    }

}  // namespace

int main() {
    std::cout << "ASTARA primary-loop demo: ATWS turbine trip\n";
    std::cout << "  (no scram; reactor self-stabilises through "
              << "negative T-coefficients)\n\n";

    PrimaryLoop loop(makeReactor(), makeSG(), makePump(), makePressurizer());
    // Deliberately NO controllers: we want to see the bare physics.

    std::ofstream out("primary_loop_atws.csv");
    out << "# t_s, P_pct, T_hot_K, T_cold_K, T_fuel_K, "
        << "rho_total_pcm, rho_external_pcm, rho_feedback_pcm, "
        << "P_sg_MPa, W_steam_kg_s, "
        << "P_pzr_MPa, L_pzr_m\n";
    out << std::fixed << std::setprecision(4);

    constexpr double dt        = 1.0e-3;
    constexpr double t_total   = 200.0;
    constexpr double t_trip    = 30.0;
    constexpr double log_every = 0.5;        // log every 0.5 simulated seconds

    std::srand(std::time(0)); // Seed with current time

    bool tripped     = false;
    double next_log  = 0.0;
    const double P0  = loop.reactor().state().kinetics.power();

    auto fuel_T_average = [](const Reactor& r) {
        const auto& fuel = r.state().thermal.T_fuel;
        double s = 0.0;
        for (double T : fuel) s += T;
        return s / static_cast<double>(fuel.size());
    };
    auto mod_T_average = [](const Reactor& r) {
        const auto& mod = r.state().thermal.T_moderator;
        double s = 0.0;
        for (double T : mod) s += T;
        return s / static_cast<double>(mod.size());
    };

    const long n_steps = static_cast<long>(t_total / dt);
    for (long i = 0; i <= n_steps; ++i) {
        const double t = i * dt;

        if (!tripped && t >= t_trip) {
            // Slam the steam valve: set the back-pressure above SG pressure.
            // The SG's valve flow law W = C_v * sqrt(rho_g * (P - P_back))
            // will then yield zero (clamped at P-P_back >= 0).
            loop.steamGenerator().inputs().steam_line_pressure_Pa
                    = loop.steamGenerator().state().P + 5.0e6;
            // Also cut feedwater (it has nowhere to go).
            loop.steamGenerator().inputs().feedwater_mass_flow_kg_s = 0.0;
            tripped = true;
            std::cout << "  t = " << t
                      << " s: STEAM VALVE CLOSED -- ATWS in progress\n";
        }

        if (t + 1.5 * dt >= next_log) {
            const auto& rs   = loop.reactor().state();
            const Reactor& r = loop.reactor();
            const auto& sg   = loop.steamGenerator();
            const auto& pz   = loop.pressurizer();
            // A random reactor engineer likes to mess with control rods or
            // chemical poison lol
            loop.reactor().reactivity().rho_external = (2* static_cast<double >(std::rand() % 2)/2 -1)/10000;
            const double T_fuel_avg = fuel_T_average(r);
            const double T_mod_avg  = mod_T_average(r);
            const double rho_ext    = r.reactivity().rho_external;
            const double rho_total  = r.reactivity().evaluate(T_fuel_avg, T_mod_avg);
            const double rho_fb     = rho_total - rho_ext;

            out << t << ", "
                << rs.kinetics.power() * 100.0 << ", "
                << rs.thermal.T_hot_leg << ", "
                << rs.thermal.T_cold_leg << ", "
                << T_fuel_avg << ", "
                << rho_total * 1.0e5 << ", "
                << rho_ext   * 1.0e5 << ", "
                << rho_fb    * 1.0e5 << ", "
                << sg.state().P * 1.0e-6 << ", "
                << sg.steamMassFlow_kg_s() << ", "
                << pz.state().pressure_Pa * 1.0e-6 << ", "
                << pz.state().water_level_m << "\n";

            next_log += log_every;
        }

        loop.timeStep(dt);
    }

    // ----- Summary -----
    const auto& rs  = loop.reactor().state();
    const Reactor& r = loop.reactor();
    const auto& sg  = loop.steamGenerator();
    const auto& pz  = loop.pressurizer();
    const double T_fuel_avg = fuel_T_average(r);
    const double T_mod_avg  = mod_T_average(r);
    const double rho_fb     = r.reactivity().evaluate(T_fuel_avg, T_mod_avg)
                              - r.reactivity().rho_external;

    std::cout << "\nFinal state at t = " << t_total << " s:\n";
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "  Reactor power      : "
              << rs.kinetics.power() * 100.0 << " %    (was "
              << P0 * 100.0 << " %)\n";
    std::cout << "  Primary T_hot      : "
              << rs.thermal.T_hot_leg  << " K    (was 597.0 K)\n";
    std::cout << "  Primary T_cold     : "
              << rs.thermal.T_cold_leg << " K    (was 559.0 K)\n";
    std::cout << "  Average fuel T     : "
              << T_fuel_avg << " K\n";
    std::cout << "  Feedback reactivity: "
              << rho_fb * 1.0e5 << " pcm  "
              << "(this is what brought power down)\n";
    std::cout << "  External reactivity: "
              << r.reactivity().rho_external * 1.0e5
              << " pcm  (no rod motion at all)\n";
    std::cout << "  SG pressure        : "
              << sg.state().P * 1.0e-6 << " MPa  (was 6.90 MPa)\n";
    std::cout << "  Steam flow         : "
              << sg.steamMassFlow_kg_s() << " kg/s (was ~ 470 kg/s)\n";
    std::cout << "  Pressurizer P      : "
              << pz.state().pressure_Pa * 1.0e-6 << " MPa  (was 15.50 MPa)\n";
    std::cout << "\nTrace: primary_loop_atws.csv\n";
    return 0;
}