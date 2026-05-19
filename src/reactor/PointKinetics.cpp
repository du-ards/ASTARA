/**
 * @file   PointKinetics.cpp
 * @brief  Implementation of the point-kinetics equations and group constants.
 */

#include "astara/reactor/PointKinetics.hpp"

#include <numeric>
#include <sstream>

namespace astara::reactor {

double DelayedGroupConstants::totalBeta() const noexcept {
    return std::accumulate(beta.begin(), beta.end(), 0.0);
}

void DelayedGroupConstants::validate() const {
    if (lambda.size() != beta.size()) {
        throw std::invalid_argument(
                "DelayedGroupConstants: lambda and beta size mismatch");
    }
    if (lambda.empty()) {
        throw std::invalid_argument(
                "DelayedGroupConstants: at least one group is required");
    }
    if (Lambda <= 0.0) {
        throw std::invalid_argument(
                "DelayedGroupConstants: Lambda (prompt-neutron generation time) "
                "must be positive");
    }
    for (std::size_t i = 0; i < lambda.size(); ++i) {
        if (lambda[i] <= 0.0) {
            std::ostringstream os;
            os << "DelayedGroupConstants: lambda[" << i
               << "] must be positive, got " << lambda[i];
            throw std::invalid_argument(os.str());
        }
        if (beta[i] < 0.0) {
            std::ostringstream os;
            os << "DelayedGroupConstants: beta[" << i
               << "] must be non-negative, got " << beta[i];
            throw std::invalid_argument(os.str());
        }
    }
    const double bt = totalBeta();
    if (bt < 0.0 || bt >= 1.0) {
        std::ostringstream os;
        os << "DelayedGroupConstants: total beta must be in [0, 1), got " << bt;
        throw std::invalid_argument(os.str());
    }
}

DelayedGroupConstants DelayedGroupConstants::u235SixGroup() noexcept {
    // Westinghouse PWR thesis values (Table A.1) -- ENDF/B-VI averages for
    // U-235 thermal fission.  The thesis lists slightly differing values for
    // beta_5/beta_6 in different tables; the values below come from the body
    // of the text and are consistent with widely-used references.
    // https://www.researchgate.net/publication/280309469_Evaluation_of_delayed_neutron_data_for_JENDL-33 
    DelayedGroupConstants g;
    g.lambda = {0.01244, 0.03054, 0.1140, 0.3014, 1.1360, 3.01400};
    g.beta   = {0.000209, 0.001414, 0.001309, 0.002727, 0.000925, 0.000273};
    g.Lambda = 1.79e-5;  // s, thesis Table A.1
    return g;
}

DelayedGroupConstants DelayedGroupConstants::oneGroupAverageOf(
        const DelayedGroupConstants& g) {
    g.validate();
    const double beta_total = g.totalBeta();
    if (beta_total <= 0.0) {
        throw std::invalid_argument(
                "oneGroupAverageOf: total beta must be positive");
    }
    // Effective decay constant: lambda_eff = beta / sum_i(beta_i / lambda_i)
    double inv_lambda_avg = 0.0;
    for (std::size_t i = 0; i < g.beta.size(); ++i) {
        inv_lambda_avg += g.beta[i] / g.lambda[i];
    }
    DelayedGroupConstants out;
    out.lambda = {beta_total / inv_lambda_avg};
    out.beta   = {beta_total};
    out.Lambda = g.Lambda;
    return out;
}

std::vector<double> steadyStatePrecursors(const DelayedGroupConstants& g,
                                          double n0) {
    g.validate();
    std::vector<double> c(g.beta.size(), 0.0);
    for (std::size_t i = 0; i < g.beta.size(); ++i) {
        c[i] = (g.beta[i] / (g.lambda[i] * g.Lambda)) * n0;
    }
    return c;
}

PointKineticsState pointKineticsDerivative(const PointKineticsState& state,
                                           double rho,
                                           const DelayedGroupConstants& g) {
    if (state.numGroups() != g.beta.size()) {
        throw std::invalid_argument(
                "pointKineticsDerivative: state and group constants disagree on G");
    }
    const std::size_t G = g.beta.size();
    const double      n = state.power();
    const auto&       c = state.precursors();
    const double      beta_total = g.totalBeta();

    PointKineticsState dy(G);
    // dn/dt = (rho - beta) / Lambda * n + sum_i lambda_i c_i
    double sum_lambda_c = 0.0;
    for (std::size_t i = 0; i < G; ++i) {
        sum_lambda_c += g.lambda[i] * c[i];
    }
    dy.power() = ((rho - beta_total) / g.Lambda) * n + sum_lambda_c;

    // dc_i/dt = beta_i / Lambda * n - lambda_i c_i
    auto& dc = dy.precursors();
    for (std::size_t i = 0; i < G; ++i) {
        dc[i] = (g.beta[i] / g.Lambda) * n - g.lambda[i] * c[i];
    }
    return dy;
}

}  // namespace astara::reactor
