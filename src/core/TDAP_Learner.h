#ifndef TDAP_H_
#define TDAP_H_

#include <Rcpp.h>
#include <omp.h>
#include "../util/Dvector.h"
#include "../util/Dmatrix.h"
#include "../util/Smatrix.h"
#include "Learner.h"

class TDAP_Learner: public Learner
{
protected:
  double u_w0;
  DVector<double> u_w;
  DMatrix<double> u_v;
  double nu_w0;
  DVector<double> nu_w;
  DMatrix<double> nu_v;
  double delta_w0;
  DVector<double> delta_w;
  DMatrix<double> delta_v;
  double h_w0;
  DVector<double> h_w;
  DMatrix<double> h_v;
  double z_w0;
  DVector<double> z_w;
  DMatrix<double> z_v;
  double alpha_w, alpha_v;
  DVector<double>* sum;

public:
  // regularization
  double l1_regw, l2_regw;
  double l1_regv, l2_regv;
  // decay rate
  double gamma;

  int max_iter;
  int random_step;

public:
  TDAP_Learner()
    : Learner(), max_iter(3000), l1_regw(0.5), l1_regv(1), l2_regw(0.1), l2_regv(0.5), alpha_w(0.1), alpha_v(0.1), random_step(1)
  {}

  ~TDAP_Learner() {}

public:
  virtual void init();
  void learn(Data& train);
  double calculate_grad_mult(double& y_hat, float& y_true);
  void calculate_param();
};

void TDAP_Learner::init()
{
  u_w0     = 0.0;
  nu_w0    = 0.0;
  delta_w0 = 0.0;
  h_w0     = 0.0;
  z_w0     = 0.0;
  u_w.setSize(fm->num_attribute); u_w.init(0.0);
  nu_w.setSize(fm->num_attribute); nu_w.init(0.0);
  delta_w.setSize(fm->num_attribute); delta_w.init(0.0);
  h_w.setSize(fm->num_attribute); h_w.init(0.0);
  z_w.setSize(fm->num_attribute); z_w.init(0.0);
  u_v.setSize(fm->num_factor, fm->num_attribute); u_v.init(0.0);
  nu_v.setSize(fm->num_factor, fm->num_attribute); nu_v.init(0.0);
  delta_v.setSize(fm->num_factor, fm->num_attribute); delta_v.init(0.0);
  h_v.setSize(fm->num_factor, fm->num_attribute); h_v.init(0.0);
  z_v.setSize(fm->num_factor, fm->num_attribute); z_v.init(0.0);
}


void TDAP_Learner::learn(Data& train)
{
  SMatrix<float>* pdata = train.data;
  DVector<float>* target = train.target;
  double egamma = exp(-gamma);

  int iter = 0;
  double y_hat, mult,  g, sigma;
  for (;;)
  {
    for (uint i = random_select(random_step); i < train.num_cases; i += random_select(random_step))
    {
      SMatrix<float>::Iterator it(*pdata, i);
      y_hat = fm->predict(it);
      mult = calculate_grad_mult(y_hat, (*target)[i]);

      if (fm->k0) {
        g = mult;
        double OLD(u_w0) = u_w0;
        u_w0 += g * g;
        nu_w0 += g;
        sigma = (sqrt(u_w0) - sqrt(OLD(u_w0))) / alpha_w;
        delta_w0 = egamma * (delta_w0 + sigma);
        h_w0 = egamma * (h_w0 + sigma * fm->w0);
        z_w0 = nu_w0 - h_w0;
      }

      if (fm->k1) {
        for (it.reset(); !it.is_end(); ++it)
        {
          g = mult * it.value;
          double& TMP(u_w) = u_w[it.index];
          double& TMP(nu_w) = nu_w[it.index];
          double& TMP(delta_w) = delta_w[it.index];
          double& TMP(h_w) = h_w[it.index];
          double OLD(u_w) = TMP(u_w);
          TMP(u_w) += g * g;
          TMP(nu_w) += g;
          sigma = (sqrt(TMP(u_w)) - sqrt(OLD(u_w))) / alpha_w;
          TMP(delta_w) = egamma * (TMP(delta_w) + sigma);
          TMP(h_w) = egamma * (TMP(h_w) + sigma * fm->w[it.index]);
          z_w[it.index] = TMP(nu_w) - TMP(h_w);
        }
      }

      for (uint f = 0; f < fm->num_factor; ++f)
      {
        double TMP(sum) = fm->m_sum[f];
        for (it.reset(); !it.is_end(); ++it)
        {
          g = mult * (TMP(sum) * it.value - fm->v(f, it.index) * it.value * it.value);
          double& TMP(u_v) = u_v(f, it.index);
          double& TMP(nu_v) = nu_v(f, it.index);
          double& TMP(delta_v) = delta_v(f, it.index);
          double& TMP(h_v) = h_v(f, it.index);
          double OLD(u_v) = TMP(u_v);
          TMP(u_v) += g * g;
          TMP(nu_v) += g;
          sigma = (sqrt(TMP(u_v)) - sqrt(OLD(u_v))) / alpha_v;
          TMP(delta_v) = egamma * (TMP(delta_v) + sigma);
          TMP(h_v) = egamma * (TMP(h_v) + sigma * fm->v(f, it.index));
          z_v(f, it.index) = TMP(nu_v) - TMP(h_v);
        }
      }

      calculate_param();

      iter ++;
      if (iter >= max_iter) { break; }
    }
    if (iter >= max_iter) { break; }
  }
}


void TDAP_Learner::calculate_param()
{
  // w0
  fm->w0 = - z_w0 / delta_w0;

  // w
  for (uint i = 0; i < fm->num_attribute; ++i)
  {
    double TMP(z_w) = z_w[i];
    if (abs(TMP(z_w)) <= l1_regw) {
      fm->w[i] = 0.0;
    } else {
      double sign = TMP(z_w) < 0.0 ? -1.0:1.0;
      fm->w[i] = - (TMP(z_w) - sign * l1_regw) / (delta_w[i] + l2_regw);
    }
  }

  // v
  for (uint f = 0; f < fm->num_factor; ++f)
  {
    for (uint i = 0; i < fm->num_attribute; ++i)
    {
      double TMP(z_v) = z_v(f, i);
      if (abs(TMP(z_v)) <= l1_regv) {
        fm->v(f, i) = 0.0;
      } else {
        double sign = TMP(z_v) < 0.0 ? -1.0:1.0;
        fm->v(f, i) = - (TMP(z_v) - sign * l1_regv) / (delta_v(f, i) + l2_regv);
      }
    }
  }
}

double TDAP_Learner::calculate_grad_mult(double& y_hat, float& y_true)
{
  double mult;
  if (fm->TASK == REGRESSION) {
    y_hat = min(max_target, y_hat);
    y_hat = max(min_target, y_hat);
    mult = -(y_true - y_hat);
  } else if (fm->TASK == CLASSIFICATION) {
    mult = - y_true * (1.0 - 1.0 / (1.0 + exp(-y_true * y_hat)));
  }
  return mult;
}

#endif
