#ifndef _CONFIG_H__
#define _CONFIG_H__
#include <string>
#include <vector>

typedef struct _task_config{ // contains info of task to execute
    int node_id;
    int dedup_num;
    int thread_cap;
    std::string name;
    int batch_size;
    float duty_cycle;
} task_config;

class ProxyPartitionWriter{
    public:
            ProxyPartitionWriter(std::string filename, int nGPU);
            ~ProxyPartitionWriter();
            bool addResults(task_config &tconfig);
            void writeResults();

    private:
            FILE * _file_to_write;
            std::vector<task_config> _vec_configs;
            int _num_of_GPU;

};
class ProxyPartitionReader{
    public:
            ProxyPartitionReader(std::string filename, int nGPU);
            ~ProxyPartitionReader();
            std::vector<task_config> getTaskConfig(int devid, int dedup_num, int thread_cap);
            std::vector<task_config> getAllTaskConfig();

    private:
            FILE *file_to_read;
            std::vector<task_config> _vec_configs;
            int _num_of_GPU;
};


#endif
