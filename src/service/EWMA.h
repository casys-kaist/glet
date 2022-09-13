#ifndef _EWMA_H__
#define _EWMA_H__



#include <stdint.h>
namespace EWMA{

	class EWMA{
		public:
			// constructor
			EWMA(int monitor_interval_ms, int query_interval_ms);
			// destructor
			~EWMA();
			// adds rate as intervals
			bool UpdateRate(uint64_t cnt);
			// add value to update
			bool UpdateValue(float value);
			// manually setup alpha to a value 
			void SetupAlpha(float alpha);
			float GetAlpha();
			float GetRate();
			void InitRate(float rate);
		private:
			float _rate;
			float _alpha;
			uint64_t _last_timestamp;
			int _interval_ms;
	}; // class EWMA


}

#else
#endif
