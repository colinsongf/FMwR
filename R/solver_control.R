solver.control <- function(max_iter = 10000, nthreads = 1, convergence = 1e-4, evaluate.method = "LL", solver = TDAP.solver())
{
  max_threads = parallel::detectCores()
  if (nthreads >= max_threads) {
    warning("nthreads is greater than the max number of threads.\n  so nthreads will be set as ", max_threads, "!\n\n", immediate. = TRUE)
    nthreads = max_threads
  }

  stopifnot(evaluate.method %in% c("AUC", "ACC", "LL", "RMSE", "MAE"))

  res <- list(nthreads = nthreads, max_iter = max_iter, convergence = convergence, evaluate.method = evaluate.method, solver = solver)
  class(res) <- "solver.control"
  res
}


MCMC.solver.default <- list(
  alpha_0   = 1.0,
  gamma_0   = 1.0,
  beta_0    = 1.0,
  mu_0      = 0.0,
  alpha     = 1.0,
  w0_mean_0 = 1.0
)

MCMC.solver <- function(...)
{
  controls <- control_assign(MCMC.solver.default, list(...))

  if (controls$is.default) {
    message("Use default MCMC solver.\n")
  }

  controls$contr
}


ALS.solver.default <- list(
  alpha_0   = 1.0,
  gamma_0   = 1.0,
  beta_0    = 1.0,
  mu_0      = 0.0,
  alpha     = 1.0,
  w0_mean_0 = 1.0
)

ALS.solver <- function(...)
{
  controls <- control_assign(ALS.solver.default, list(...))

  if (controls$is.default) {
    message("Use default ALS solver.\n")
  }

  controls$contr
}


SGD.solver.default <- list(
  learn_rate  = 0.01,
  random_step = 1L
)

SGD.solver <- function(...)
{
  controls <- control_assign(SGD.solver.default, list(...))

  if (controls$is.default) {
    message("Use default SGD solver.\n")
  }

  controls$contr
}

FTRL.solver.default <- list(
  alpha_w     = 0.1,
  alpha_v     = 0.1,
  beta_w      = 1.0,
  beta_v      = 1.0,
  random_step = 1L
)

FTRL.solver <- function(...)
{
  controls <- control_assign(FTRL.solver.default, list(...))

  if (controls$is.default) {
    message("Use default FTRL solver. \n")
  }

  controls$contr
}

TDAP.solver.default <- list(
  gamma       = 1e-4,
  alpha_w     = 0.1,
  alpha_v     = 0.1,
  random_step = 1L
)

TDAP.solver <- function(...)
{
  controls <- control_assign(TDAP.solver.default, list(...))

  if (controls$is.default) {
    message("Use default TDAP solver. \n")
  }

  controls$contr
}
