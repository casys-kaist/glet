#include "EWMA.h"
#include <assert.h>
#include <stdlib.h>
#include <iostream>

namespace EWMA {
	EWMA::EWMA(int monitor_interval_ms, int query_interval_ms){
		// aplha = 1/(N+1), w/ N = number of data for calculating moving average
		float N = std::max(float(query_interval_ms)/monitor_interval_ms, static_cast<float>(1.0));
		_alpha=2/(N+1);
		assert(0<= _alpha && _alpha <=1);
		_rate=0;
		_last_timestamp=0;
		_interval_ms = monitor_interval_ms;
	}
	EWMA::~EWMA() {}
	// adds rate as intervals
	bool EWMA::UpdateRate(uint64_t cnt){ // recieves count for interval_ms
		float rate = float(cnt) *1000 / _interval_ms; // req/s
		_rate = _alpha * rate + (1.0-_alpha) * _rate;
	}
	bool EWMA::UpdateValue(float value){ // recieves count for interval_ms
		_rate = _alpha * value + (1.0-_alpha) * _rate;
	}
	// manually setup alpha to a value 
	void EWMA::SetupAlpha(float alpha){
		assert(0 <= alpha && alpha <=1);
		_alpha=alpha;
	}
	float EWMA::GetAlpha(){
		return _alpha;
	}

	float EWMA::GetRate(){
		return _rate;
	}

	void EWMA::InitRate(float rate){
		_rate=rate;
#ifdef EPOCH_DEBUG
		std::cout << "InitRate: _rate initiated to : " << _rate << std::endl;
#endif 
	}
}
