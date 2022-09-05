#ifndef _PROFILE_H__
#define _PROFILE_H__
#include <cstdint>

class ReqProfile{
    public: 
        ReqProfile(int req_id, int job_id);
        ~ReqProfile();
        void setInputStart(uint64_t time);
        void setInputEnd(uint64_t time);
        void setCPUCompStart(uint64_t time);
        void setGPUStart(uint64_t time);
        void setGPUEnd(uint64_t time);
        void setCPUPostEnd(uint64_t time);
        void setOutputStart(uint64_t time);
        void setOutputEnd(uint64_t time);
        void setBatchSize(int size);
        void printTimes();
        
    private:
        uint64_t _input__start=0;
        uint64_t _input_end=0;
        uint64_t _cpu_comp_start=0;
        uint64_t _gpu_start=0;
        uint64_t _gpu_end=0;
        uint64_t _cpu_post_end=0;
        uint64_t _output_start=0;
        uint64_t _output_end=0;
        int _id;
        int _jid;
        int _batch_size;
};

#endif 
