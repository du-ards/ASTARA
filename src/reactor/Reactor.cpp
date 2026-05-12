/**
 * @file   Reactor.cpp
 * @brief  Implementation of the composite Reactor (kinetics + thermal +
 *         feedback reactivity), advanced with classical RK4.
 */

#include "astara/reactor/Reactor.hpp"
#include "astara/core/Integrator.hpp"

#include <cmath>
#include <stdexcept>
#include <utility>

namespace astara::reactor {

// =============================================================================
// ReactorState arithmetic
// =============================================================================

ReactorState& ReactorState::operator+=(const ReactorState& rhs) {
    kinetics += rhs.kinetics;
    thermal  += rhs.thermal;
    return *this;
}
ReactorState& ReactorState::operator-=(const ReactorState& rhs) {
    kinetics -= rhs.kinetics;
    thermal  -= rhs.thermal;
    return *this;
}
ReactorState& ReactorState::operator*=(double s) noexcept {
    kinetics *= s;
    thermal  *= s;
    return *this;
}

// =============================================================================
// Reactor
// =============================================================================

Reactor::Reactor(DelayedGroupConstants    groups,
                 ReactorThermalParameters thermal_parameters,
                 double                   rated_thermal_power_W,
                 ReactivityModel          reactivity)
        : groups_(std::move(groups)),
          tp_(std::move(thermal_parameters)),
          P_rated_W_(rated_thermal_power_W),
          reactivity_(reactivity) {
    groups_.validate();
    tp_.validate();
    if (!(P_rated_W_ > 0.0)) {
        throw std::invalid_argument("Reactor: rated_thermal_power_W must be positive");
    }
    state_.kinetics = PointKineticsState(groups_.beta.size());
}

void Reactor::initialiseSteadyState(double n0, double T_inlet_K) {
    if (!(n0 >= 0.0)) {
        throw std::invalid_argument("Reactor::initialiseSteadyState: n0 must be non-negative");
    }
    if (!(T_inlet_K > 0.0)) {
        throw std::invalid_argument("Reactor::initialiseSteadyState: T_inlet_K must be > 0 K");
    }

    // Kinetics: precursors at steady-state for the given power.
    state_.kinetics = PointKineticsState(groups_.beta.size());
    state_.kinetics.power() = n0;
    state_.kinetics.precursors() = steadyStatePrecursors(groups_, n0);

    // Thermal: solve the steady-state temperature field.
    const double P_thermal_W = n0 * P_rated_W_;
    state_.thermal = steadyStateThermal(tp_, P_thermal_W, T_inlet_K);

    // Reset reactivity references to the resulting averages so that initial
    // feedback contribution is zero.  External rho is preserved.
    reactivity_.T_fuel_reference_K      = state_.thermal.averageFuelTemperature();
    reactivity_.T_moderator_reference_K = state_.thermal.averageModeratorTemperature();
    T_inlet_K_ = T_inlet_K;
    state_.t_s = 0.0;
}

ReactorState Reactor::evaluateDerivative(const ReactorState& s) const {
    const double T_F_avg = s.thermal.averageFuelTemperature();
    const double T_M_avg = s.thermal.averageModeratorTemperature();
    const double rho     = reactivity_.evaluate(T_F_avg, T_M_avg);

    ReactorState dy;
    dy.kinetics = pointKineticsDerivative(s.kinetics, rho, groups_);
    const double P_thermal_W = s.kinetics.power() * P_rated_W_;
    dy.thermal  = reactorThermalDerivative(s.thermal, tp_, P_thermal_W, T_inlet_K_);
    dy.t_s      = 0.0;  // time advances monotonically; not part of vector ops
    return dy;
}

void Reactor::timeStep(double dt) {
    if (!(dt > 0.0)) {
        throw std::invalid_argument("Reactor::timeStep: dt must be positive");
    }

    auto deriv = [this](double /*t*/, const ReactorState& s) {
        return this->evaluateDerivative(s);
    };

    state_ = astara::core::rk4Step(state_.t_s, state_, dt, deriv);
    state_.t_s += dt;

    // Sanity checks. Failing loudly is better.
    if (!std::isfinite(state_.kinetics.power())) {
        throw std::runtime_error("Reactor::timeStep produced non-finite power");
    }
    auto check_blowups=[](std::vector<double>& params, std::string message){
        for (double v : params) {
            if (!std::isfinite(v)) 
                throw std::runtime_error(message);
        }
    }
    check_blowups(state_.kinetics.precursors(),"Reactor::timeStep produced non-finite precursor");
    check_blowups(state_.thermal.T_fuel,"Reactor::timeStep produced non-finite fuel T");
    check_blowups(state_.thermal.T_moderator,"Reactor::timeStep produced non-finite moderator T");

}

}  // namespace astara::reactor
