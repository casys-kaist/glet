#ifndef EXP_MODEL_H__
#define EXP_MODEL_H__

class ExpModel
{

public:
    ExpModel();
    ~ExpModel();

    double randomExponentialInterval(double mean, int nclient, unsigned int seed);

    private:
};


#else
#endif
