#include "baseLogLikelihood.h"
#include <boost/math/quadrature/gauss_kronrod.hpp>
#include <boost/math/special_functions/bessel.hpp>
#include <boost/math/special_functions/gamma.hpp>

const double BaseLogLikelihood::m_Epsilon = 1.0e-4;

void BaseLogLikelihood::SetNeighborhood(const unsigned int n)
{
  std::vector<int> workVec(n, 0);
  m_Neighborhood.resize(1);
  m_Neighborhood[0] = workVec;
  NeighborhoodType workList;

  for (unsigned int i = 0;i < n;++i)
  {
    workList = m_Neighborhood;
    m_Neighborhood.clear();

    for (unsigned int j = 0;j < workList.size();++j)
    {
      for (int k = -1;k <= 1;++k)
      {
        workVec = workList[j];
        workVec[i] += k;
        m_Neighborhood.push_back(workVec);
      }
    }
  }
}

std::vector<arma::rowvec> BaseLogLikelihood::GetTrialVectors(const arma::rowvec &x, const arma::vec &lb, const arma::vec &ub)
{
  unsigned int numTrials = m_Neighborhood.size();
  std::vector<arma::rowvec> trialVectors(numTrials);
  std::vector<int> workNeighborhood;
  arma::rowvec workVector;

  for (unsigned int i = 0;i < numTrials;++i)
  {
    workNeighborhood = m_Neighborhood[i];
    workVector = x;
    for (unsigned int j = 0;j < m_DomainDimension;++j)
      workVector[j] += (double)workNeighborhood[j] * (ub[j] - lb[j]);
    trialVectors[i] = workVector;
  }

  return trialVectors;
}

void BaseLogLikelihood::SetInputs(
    const arma::mat &points,
    const arma::uvec &labels,
    const arma::vec &lb,
    const arma::vec &ub)
{
  m_DomainDimension = points.n_cols;
  m_SampleSize = points.n_rows;
  m_PointLabels = labels;
  m_DomainVolume = 1.0;
  for (unsigned int i = 0;i < m_DomainDimension;++i)
    m_DomainVolume *= (ub[i] - lb[i]);

  this->SetNeighborhood(m_DomainDimension);
  m_DistanceMatrix.set_size(m_SampleSize, m_SampleSize);
  m_DistanceMatrix.fill(0.0);
  std::vector<arma::rowvec> trialVectors;
  arma::rowvec workVec1, workVec2;

  for (unsigned int i = 0;i < m_SampleSize;++i)
  {
    workVec1 = points.row(i);

    if (m_UsePeriodicDomain)
      trialVectors = this->GetTrialVectors(workVec1, lb, ub);

    for (unsigned int j = i + 1;j < m_SampleSize;++j)
    {
      workVec2 = points.row(j);

      double workDistance = 0.0;

      if (m_UsePeriodicDomain)
      {
        for (unsigned int k = 0;k < trialVectors.size();++k)
        {
          double testDistance = arma::norm(trialVectors[k] - workVec2);
          if (testDistance < workDistance || k == 0)
            workDistance = testDistance;
        }
      }
      else
        workDistance = arma::norm(workVec1 - workVec2);

      m_DistanceMatrix(i, j) = workDistance;
      m_DistanceMatrix(j, i) = workDistance;
    }
  }

  // Rcpp::Rcout << "Domain Dimension: " << m_DomainDimension << std::endl;
  // Rcpp::Rcout << "Domain Volume: " << m_DomainVolume << std::endl;
  // Rcpp::Rcout << "Sample size: " << m_SampleSize << std::endl;
  // Rcpp::Rcout << "Point labels: " << m_PointLabels.as_row() << std::endl;
}

arma::mat BaseLogLikelihood::GetInitialPoint()
{
  arma::mat params(this->GetNumberOfParameters(), 1);
  return params;
}

unsigned int BaseLogLikelihood::GetNumberOfParameters()
{
  unsigned int numParams = 4;

  if (m_EstimateIntensities)
    numParams += 2;

  return numParams;
}

double BaseLogLikelihood::GetIntegral()
{
  typedef boost::math::quadrature::gauss_kronrod<double, 61> QuadratureType;
  const double lBound = 0.0;
  const double uBound = std::numeric_limits<double>::infinity();

  BaseIntegrand integrand;
  integrand.SetKFunction(this->GetKFunction());
  integrand.SetFirstAlpha(m_FirstAlpha);
  integrand.SetSecondAlpha(m_SecondAlpha);
  integrand.SetInverseCrossAlpha(m_InverseCrossAlpha);
  integrand.SetFirstAmplitude(m_FirstAmplitude);
  integrand.SetSecondAmplitude(m_SecondAmplitude);
  integrand.SetCrossAmplitude(m_CrossAmplitude);
  integrand.SetDomainDimension(m_DomainDimension);
  auto GetIntegrandValue =              [&integrand](const double &t){return integrand(t);};
  auto GetDerivativeWRTFirstAlpha =     [&integrand](const double &t){return integrand.GetDerivativeWRTFirstAlpha(t);};
  auto GetDerivativeWRTCrossAlpha =     [&integrand](const double &t){return integrand.GetDerivativeWRTCrossAlpha(t);};
  auto GetDerivativeWRTSecondAlpha =    [&integrand](const double &t){return integrand.GetDerivativeWRTSecondAlpha(t);};
  auto GetDerivativeWRTCrossIntensity = [&integrand](const double &t){return integrand.GetDerivativeWRTCrossIntensity(t);};

  double resVal = 2.0 * M_PI * QuadratureType::integrate(GetIntegrandValue, lBound, uBound);

  m_GradientIntegral.set_size(this->GetNumberOfParameters());
  m_GradientIntegral[0] = 2.0 * M_PI * QuadratureType::integrate(GetDerivativeWRTFirstAlpha,     lBound, uBound);
  m_GradientIntegral[1] = 2.0 * M_PI * QuadratureType::integrate(GetDerivativeWRTCrossAlpha,     lBound, uBound);
  m_GradientIntegral[2] = 2.0 * M_PI * QuadratureType::integrate(GetDerivativeWRTSecondAlpha,    lBound, uBound);
  m_GradientIntegral[3] = 2.0 * M_PI * QuadratureType::integrate(GetDerivativeWRTCrossIntensity, lBound, uBound);

  return resVal;
}

double BaseLogLikelihood::GetLogDeterminant()
{
  arma::mat lMatrix(m_SampleSize, m_SampleSize);
  arma::mat lMatrixDeriv1(m_SampleSize, m_SampleSize);
  arma::mat lMatrixDeriv2(m_SampleSize, m_SampleSize);
  arma::mat lMatrixDeriv3(m_SampleSize, m_SampleSize);
  arma::mat lMatrixDeriv4(m_SampleSize, m_SampleSize);
  double resVal = 0.0;
  double workValue1 = 0.0;
  double workValue2 = 0.0;
  double workValue3 = 0.0;
  double workValue4 = 0.0;
  double workSign = 0.0;

  for (unsigned int i = 0;i < m_SampleSize;++i)
  {
    for (unsigned int j = i;j < m_SampleSize;++j)
    {
      double sqDist = m_DistanceMatrix(i, j) * m_DistanceMatrix(i, j);
      unsigned int workLabel = m_PointLabels[i] + m_PointLabels[j];
      double tmpVal = this->EvaluateL12Function(sqDist, m_FirstAmplitude, m_SecondAmplitude, m_CrossAmplitude, m_InverseCrossAlpha, m_DomainDimension);

      if (workLabel == 2)
        resVal = this->EvaluateLFunction(sqDist, m_FirstAmplitude, m_CrossAmplitude, m_FirstAlpha, tmpVal, m_DomainDimension);
      else if (workLabel == 3)
        resVal = tmpVal;
      else
        resVal = this->EvaluateLFunction(sqDist, m_SecondAmplitude, m_CrossAmplitude, m_SecondAlpha, tmpVal, m_DomainDimension);

      lMatrix(i, j) = resVal;
      lMatrixDeriv1(i, j) = workValue1;
      lMatrixDeriv2(i, j) = workValue2;
      lMatrixDeriv3(i, j) = workValue3;
      lMatrixDeriv4(i, j) = workValue4;

      if (i != j)
      {
        lMatrix(j, i) = resVal;
        lMatrixDeriv1(j, i) = workValue1;
        lMatrixDeriv2(j, i) = workValue2;
        lMatrixDeriv3(j, i) = workValue3;
        lMatrixDeriv4(j, i) = workValue4;
      }
    }
  }

  arma::log_det(resVal, workSign, lMatrix);
  arma::mat lMatrixInverse = arma::inv(lMatrix);

  m_GradientLogDeterminant.set_size(this->GetNumberOfParameters());
  m_GradientLogDeterminant[0] = arma::trace(lMatrixInverse * lMatrixDeriv1);
  m_GradientLogDeterminant[1] = arma::trace(lMatrixInverse * lMatrixDeriv2);
  m_GradientLogDeterminant[2] = arma::trace(lMatrixInverse * lMatrixDeriv3);
  m_GradientLogDeterminant[3] = arma::trace(lMatrixInverse * lMatrixDeriv4);

  return resVal;
}

double BaseLogLikelihood::Evaluate(const arma::mat& x)
{
  this->SetModelParameters(x);

  if (m_Modified)
  {
    m_Integral = this->GetIntegral();
    m_LogDeterminant = this->GetLogDeterminant();
  }

  if (!std::isfinite(m_Integral) || !std::isfinite(m_LogDeterminant))
  {
    Rcpp::Rcout << m_Integral << " " << m_LogDeterminant << " " << x.as_row() << std::endl;
    Rcpp::stop("Non finite stuff in evaluate");
  }

  double logLik = 2.0 * m_DomainVolume;
  logLik += m_DomainVolume * m_Integral;
  logLik += m_LogDeterminant;

  return -2.0 * logLik;
}

void BaseLogLikelihood::Gradient(const arma::mat& x, arma::mat &g)
{
  this->SetModelParameters(x);
  g.set_size(this->GetNumberOfParameters(), 1);

  bool validParams = this->CheckModelParameters();
  if (!validParams)
  {
    g.fill(0.0);
    return;
  }

  if (m_Modified)
  {
    m_Integral = this->GetIntegral();
    m_LogDeterminant = this->GetLogDeterminant();
  }

  if (!std::isfinite(m_Integral) || !std::isfinite(m_LogDeterminant))
  {
    Rcpp::Rcout << m_Integral << " " << m_LogDeterminant << " " << x.as_row() << std::endl;
    Rcpp::stop("Non finite stuff in gradient");
  }

  for (unsigned int i = 0;i < this->GetNumberOfParameters();++i)
    g[i] = m_DomainVolume * m_GradientIntegral[i] + m_GradientLogDeterminant[i];

  g *= -2.0;
}

double BaseLogLikelihood::EvaluateWithGradient(const arma::mat& x, arma::mat& g)
{
  this->SetModelParameters(x);
  g.set_size(this->GetNumberOfParameters(), 1);

  bool validParams = this->CheckModelParameters();
  if (!validParams)
  {
    g.fill(0.0);
    return DBL_MAX;
  }

  m_Integral = this->GetIntegral();
  m_LogDeterminant = this->GetLogDeterminant();

  if (!std::isfinite(m_Integral) || !std::isfinite(m_LogDeterminant))
  {
    Rcpp::Rcout << m_Integral << " " << m_LogDeterminant << " " << x.as_row() << std::endl;
    Rcpp::stop("Non finite stuff in evaluate with gradient");
  }

  double logLik = 2.0 * m_DomainVolume;
  logLik += m_DomainVolume * m_Integral;
  logLik += m_LogDeterminant;

  for (unsigned int i = 0;i < this->GetNumberOfParameters();++i)
    g[i] = m_DomainVolume * m_GradientIntegral[i] + m_GradientLogDeterminant[i];

  g *= -2.0;

  return -2.0 * logLik;
}

double BaseLogLikelihood::EvaluateConstraint(const size_t i, const arma::mat& x)
{
  this->SetModelParameters(x);
  this->CheckModelParameters();
  return m_ConstraintVector[i];
}

void BaseLogLikelihood::GradientConstraint(const size_t i, const arma::mat& x, arma::mat& g)
{
  g.set_size(this->GetNumberOfParameters(), 1);
  g.fill(0.0);
}

void BaseLogLikelihood::SetIntensities(const double rho1, const double rho2)
{
  m_FirstIntensity = rho1;
  m_SecondIntensity = rho2;
  m_EstimateIntensities = false;
}

void BaseLogLikelihood::SetModelParameters(const arma::mat &params)
{
  m_Modified = false;
  unsigned int pos = 0;
  double workScalar = 0.0;

  // Set k1
  workScalar = params[pos];

  if (m_FirstAmplitude != workScalar)
  {
    m_FirstAmplitude = workScalar;
    if (!m_EstimateIntensities)
      m_FirstAlpha = this->RetrieveAlphaFromParameters(m_FirstAmplitude, m_FirstIntensity, m_DomainDimension);
    m_Modified = true;
  }

  ++pos;

  // Set k2
  workScalar = params[pos];

  if (m_SecondAmplitude != workScalar)
  {
    m_SecondAmplitude = workScalar;
    if (!m_EstimateIntensities)
      m_SecondAlpha = this->RetrieveAlphaFromParameters(m_SecondAmplitude, m_SecondIntensity, m_DomainDimension);
    m_Modified = true;
  }

  ++pos;

  // Set k12star
  workScalar = params[pos];

  if (m_NormalizedCrossAmplitude != workScalar)
  {
    m_NormalizedCrossAmplitude = workScalar;
    m_Modified = true;
  }

  // double upperBound = (1.0 - m_FirstAmplitude) * (1.0 - m_SecondAmplitude);
  // upperBound = std::min(upperBound, m_FirstAmplitude * m_SecondAmplitude);
  // upperBound = std::max(upperBound, 0.0);
  // upperBound = std::sqrt(upperBound);
  // m_CrossAmplitude = 0.55555555 * upperBound;

  ++pos;

  // Set beta12
  workScalar = params[pos];

  if (m_CrossBeta != workScalar)
  {
    m_CrossBeta = workScalar;
    m_Modified = true;
  }

  // m_InverseCrossAlpha = 0.6;

  ++pos;

  // Set alpha_i_star
  if (m_EstimateIntensities)
  {
    double gammaValue = boost::math::tgamma(1.0 + (double)m_DomainDimension / 2.0);
    double upperBound = std::pow(m_DomainVolume / gammaValue, 1.0 / (double)m_DomainDimension);
    upperBound /= std::sqrt(2.0 * M_PI / (double)m_DomainDimension);

    workScalar = params[pos];

    if (m_NormalizedFirstAlpha != workScalar)
    {
      m_NormalizedFirstAlpha = workScalar;
      m_FirstAlpha = m_NormalizedFirstAlpha * upperBound;
      m_Modified = true;
    }

    ++pos;

    workScalar = params[pos];

    if (m_NormalizedSecondAlpha != workScalar)
    {
      m_NormalizedSecondAlpha = workScalar;
      m_SecondAlpha = m_NormalizedSecondAlpha * upperBound;
      m_Modified = true;
    }
  }

  if (m_Modified)
  {
    m_InverseCrossAlpha = m_CrossBeta / this->GetCrossAlphaLowerBound();
    double upperBound = (1.0 - m_FirstAmplitude) * (1.0 - m_SecondAmplitude);
    upperBound = std::min(upperBound, m_FirstAmplitude * m_SecondAmplitude);
    upperBound = std::max(upperBound, 0.0);
    upperBound = std::sqrt(upperBound);
    m_CrossAmplitude = m_NormalizedCrossAmplitude * upperBound;
    if (m_EstimateIntensities)
    {
      m_FirstIntensity = this->RetrieveIntensityFromParameters(m_FirstAmplitude, m_FirstAlpha, m_DomainDimension);
      m_SecondIntensity = this->RetrieveIntensityFromParameters(m_SecondAmplitude, m_SecondAlpha, m_DomainDimension);
    }
  }

  // Rcpp::Rcout << m_FirstAlpha << " " << m_SecondAlpha << " " << m_InverseCrossAlpha << " " << m_FirstIntensity << " " << m_SecondIntensity << " " << m_FirstAmplitude << " " << m_SecondAmplitude << " " << m_CrossAmplitude << std::endl;
}

bool BaseLogLikelihood::CheckModelParameters()
{
  return true;
}

double BaseLogLikelihood::GetBesselJRatio(const double sqDist, const double alpha, const unsigned int dimension, const bool cross)
{
  // if cross is true, alpha is in fact its inverse
  double order = (double)dimension / 2.0;
  double tmpVal = (cross) ? alpha : 1.0 / alpha;
  tmpVal *= std::sqrt(2.0 * (double)dimension * sqDist);

  if (tmpVal < std::sqrt(std::numeric_limits<double>::epsilon()))
    return 1.0 / boost::math::tgamma(1.0 + order);

  // if (tmpVal > 1.0e5)
  // {
  //   double resVal = (cross) ? std::cos(tmpVal - M_PI / (2.0 * alpha) - M_PI / 4.0) : std::cos(tmpVal - alpha * M_PI / 2.0 - M_PI / 4.0);
  //   resVal *= (std::pow(2.0, order) * std::sqrt(2.0 / M_PI)  / std::pow(tmpVal, 0.5 + order));
  //   return resVal;
  // }

  return boost::math::cyl_bessel_j(order, tmpVal) / std::pow(tmpVal / 2.0, order);
}
