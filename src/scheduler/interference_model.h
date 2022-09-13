#ifndef _INTERFERENCE_MODEL_H__
#define _INTERFERENCE_MODEL_H__

#include <string>
#include <sstream>
#include <fstream>
#include <map>
#include <vector>
#include <utility>
#include <cmath>

typedef struct _InterferenceModelInfo{
	std::map<int, std::pair<float,float>> parameters;
	int id; 
	std::string name;
} InterferenceModelInfo;

namespace InterferenceModeling {
	class InterferenceModel{
		public:
			void setup(std::string model_const_file, std::string util_file);
			double getInterference(std::string my_model, int my_batch, int my_partition, std::string your_model, int your_batch, int your_partition);
		private:
			void _setup(std::string input_file1, std::string input_file2);
			std::vector<double> _intModelConsts;
			std::map<std::pair<std::string, int>,std::vector<double>> _utilInfo;

	};
}
#endif
