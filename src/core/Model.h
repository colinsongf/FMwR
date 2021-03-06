#ifndef FM_MODEL_H_
#define FM_MODEL_H_

#include <Rcpp.h>
#ifdef _OPENMP
#include <omp.h>
#endif
#include <assert.h>
#include <omp.h>
#include "../util/Dvector.h"
#include "../util/Dmatrix.h"
#include "../util/Smatrix.h"
#include "../util/Random.h"
#include "Data.h"


//TODO: 添加SOLVER
using namespace Rcpp;

class Model
{
public:
  int SOLVER;
  int TASK;

public:
  DVector<double> m_sum;
  DVector<double> m_sum_sqr;

public:
  double w0;
  DVectorDouble w;
  DMatrixDouble v;

public:
  int nthreads; // predict in parallel run type
  uint num_attribute;
  bool k0, k1;
  uint num_factor;
  double l1_regw, l1_regv;
  double l2_reg0, l2_regw, l2_regv;
  // double reg0, regw, regv;
  double init_mean, init_stdev;

public:
  Model()
    : SOLVER(TDAP), TASK(CLASSIFICATION), num_factor(0), init_mean(0.0), init_stdev(0.01),l1_regw(0.0), l1_regv(0.0), l2_reg0(0.0), l2_regw(0.0), l2_regv(0.0), k0(true), k1(true), nthreads(1) //rename keep_w0
  {}

  ~Model() {}

public:
  void init();
  double predict(SMatrix<float>::Iterator& x);
  void predict_batch(Data& _data, DVector<double>& _out, DVector<double>* v_sum_ = NULL);
  void predict_prob(Data& _data, DVector<double>& _out);
  List save_model();
  void load_model(List model_list);
};

void Model::init()
{
  w0 = 0.0;
  w.setSize(num_attribute);
  w.init(0.0);
  v.setSize(num_factor, num_attribute);
  v.init_norm(init_mean, init_stdev);
  m_sum.setSize(num_factor);
  m_sum_sqr.setSize(num_factor);
}


double Model::predict(SMatrix<float>::Iterator& x)
{
  double pred = 0.0;
  if (k0) { pred += w0; }
  // DVector<double> sum(num_factor);
  // DVector<double> sum_sqr(num_factor);
  m_sum.init(0.0);
  m_sum_sqr.init(0.0);
  for ( ; !x.is_end(); ++x)
  {
    double _val = x.value;
    uint _idx = x.index;
    // if (_idx >= num_attribute) { stop("Length of x is greater then then number of attributes..."); }
    if (k1) pred += w[_idx] * _val;

    double* it_v = v.begin() + _idx;
    for (uint i = 0; i < num_factor; ++i)
    {
      double _tmp= *it_v * _val;
      m_sum[i] += _tmp;
      m_sum_sqr[i] += _tmp * _tmp;
      it_v += num_attribute;
    }
  }

  for (uint i = 0; i < num_factor; ++i) { pred += 0.5 * (m_sum[i] * m_sum[i] - m_sum_sqr[i]); }

  return pred;
}


void Model::predict_batch(Data& _data, DVector<double>& _out, DVector<double>* v_sum_)
{
  SMatrix<float>* pdata = _data.data;
  uint _nr = pdata->nrow();
  uint _nc = pdata->ncol();

  if (_nr != _out.size()) { stop("length of output vector is not correct..."); }
  if ((w.size() != _nc) || _nc != v.ncol()) { stop("number of input's features is not correct..."); }

  if (k0) { _out.init(w0); }
  else { _out.init(0.0); }

  uint begin, end, f, j;
  double w_sum, v_sum, v_sum_sqr, v_res, tmp_;

  if (k1)
  {
    #pragma omp parallel for num_threads(nthreads) private(j, end, w_sum)
    for (uint i = 0; i < _nr; ++i)
    {
      end = pdata->row_idx[i+1];
      w_sum = 0.0;
      for (j = pdata->row_idx[i]; j < end; ++j)
      {
        w_sum += w[pdata->col_idx[j]] * pdata->value[j];
      }
      _out[i] += w_sum;
    }
  }

  if (num_factor > 0)
  {
    #pragma omp parallel for num_threads(nthreads) private(v_res, v_sum, v_sum_sqr, begin, end, f, j, tmp_)
    for (uint i = 0; i < _nr; ++i)
    {
      end = pdata->row_idx[i+1];
      begin = pdata->row_idx[i];
      v_res = 0.0;
      for (f = 0; f < num_factor; ++f)
      {
        v_sum = 0; v_sum_sqr = 0;
        for (j = begin; j < end; ++j)
        {
          tmp_ = pdata->value[j] * v(f, pdata->col_idx[j]);
          v_sum += tmp_;
          v_sum_sqr += tmp_ * tmp_;
        }
        v_res += (0.5*v_sum*v_sum - 0.5*v_sum_sqr);
        if (v_sum_ != NULL) {
        (*v_sum_)[f] = v_sum; //just for SGD
        }
      }
      _out[i] += v_res;
    }
  }
}

void Model::predict_prob(Data& _data, DVector<double>& _out)
{
  predict_batch(_data, _out);
  if (SOLVER == MCMC || SOLVER == ALS) {
    #pragma omp parallel for num_threads(nthreads)
    for (uint i = 0; i < _out.size(); ++i)
    {
      _out[i] = fast_pnorm(_out[i]);
    }
  } else {
    #pragma omp parallel for num_threads(nthreads)
    for (uint i = 0; i < _out.size(); ++i)
    {
      _out[i] = 1.0 / (1.0 + exp(-_out[i]));
    }
  }
}

List Model::save_model()
{
  return List::create(
    _["model.control"] = List::create(
      _["keep.w0"]       = k0,
      _["L2.w0"]         = l2_reg0,
      _["keep.w1"]       = k1,
      _["L1.w1"]         = l1_regw,
      _["L2.w1"]         = l2_regw,
      _["factor.number"] = num_factor,
      _["v.init_mean"]   = init_mean,
      _["v.init_stdev"]  = init_stdev,
      _["L1.v"]          = l1_regv,
      _["L2.v"]          = l2_regv
    ),
    _["model.params"] = List::create(
      _["w0"] = w0,
      _["w"]  = w.to_rtype(),
      _["v"]  = v.to_rtype()
    )
  );
}

void Model::load_model(List model_list)
{
  List model_control = model_list["model.control"];
  k0                 = (bool)(model_control["keep.w0"]);
  l2_reg0            = (double)(model_control["L2.w0"]);
  k1                 = (bool)(model_control["keep.w1"]);
  l1_regw            = (double)(model_control["L1.w1"]);
  l2_regw            = (double)(model_control["L2.w1"]);
  num_factor         = (int)(model_control["factor.number"]);
  init_mean          = (double)(model_control["v.init_mean"]);
  init_stdev         = (double)(model_control["v.init_stdev"]);
  l1_regv            = (double)(model_control["l1_regv"]);
  l2_regv            = (double)(model_control["l2_regv"]);
  List model_params  = model_list["model.params"];
  w0                 = (double)(model_params["w0"]);
  w.assign(as<NumericVector>(model_params["w"]));
  v.assign(as<NumericMatrix>(model_params["v"]));
}

#endif
