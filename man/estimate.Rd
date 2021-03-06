% Generated by roxygen2: do not edit by hand
% Please edit documentation in R/pcf.R
\name{estimate}
\alias{estimate}
\title{Estimation of Stationary Bivariate 2-dimensional DPP}
\usage{
estimate(
  X,
  model = "Gauss",
  method = "PCF",
  rmin_alpha = 22,
  rmin_alpha12 = 22,
  rmin_tau = 22,
  tau_min = 0.1,
  p = 0.5,
  divisor_marginal = "d",
  divisor_cross = "d",
  bw_marginal = "SJ",
  bw_cross = "SJ",
  use_polar_marginal = FALSE,
  use_polar_cross = FALSE
)
}
\arguments{
\item{X}{a \code{\link[spatstat]{ppp}} object storing the point pattern.}

\item{rmin_alpha}{The lower bound on distances that should be taken into
account for estimating marginal alpha parameters (default: index 1).}

\item{rmin_alpha12}{The lower bound on distances that should be taken into
account for estimating the crossing alpha parameter (default: index 1).}

\item{rmin_tau}{The lower bound on distances that should be taken into
account for estimating the correlation (default: index 31).}

\item{tau_min}{Correlation value used to compute a suitable upper bound for
the crossing alpha parameter (default: 0.1).}

\item{p}{Power used in the marginal contrasts for estimating \code{alpha1}
and \code{alpha2} (default: 0.2).}
}
\value{
A list with the estimated model parameters in the following order:
\code{rho1}, \code{rho2}, \code{alpha1}, \code{alpha2}, \code{alpha12} and
\code{tau}.
}
\description{
Estimation of Stationary Bivariate 2-dimensional DPP
}
\examples{
res <- purrr::map_df(sim_gauss5, estimate)
boxplot(res$alpha1)
abline(h = 0.03, col = "red")
boxplot(res$alpha2)
abline(h = 0.03, col = "red")
boxplot(res$alpha12)
abline(h = 0.035, col = "red")
boxplot(res$tau)
abline(h = 0.5, col = "red")
}
