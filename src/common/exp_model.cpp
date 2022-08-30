#include "exp_model.h"
#include <random>

ExpModel::ExpModel(){
}

ExpModel::~ExpModel(){
}
double ExpModel::randomExponentialInterval(double mean,int nclient,unsigned int seed) {
	  static thread_local std::mt19937* rng = new std::mt19937(seed);
	    std::uniform_real_distribution<double> dist(0, 1.0);
	     double _mean = nclient * mean;
	      /* Cap the lower end so that we don't return infinity */
	      return - log(std::max(dist(*rng), 1e-9)) * _mean;
}

