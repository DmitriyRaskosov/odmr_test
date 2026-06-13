#include <fstream>
struct CV_config{
    int num_repeats;
    int t1;
    int t2;
    int t4;
    int t5;
};
struct Rigol_config_cv{
    int gain;
    double start_freq;   // в Гц
    double stop_freq;    // в Гц
    double freq_step;    // в Гц
};

struct Rabi_config{
    int N;
    int t1;
    int t2_start;
    int t2_end;
    int t2_step;
    int t3;
    int t4;
    int t5;
    int t6;
    int t7;
    int delay_between_measurements;
};
struct Rigol_config_rabi{
    int gain;
    double freq;   // в Гц
};
struct Impulse_config{
    int N;
    int t1;
    int t2_start;
    int t2_end;
    int t2_step;
    int t3;
    int t4;
    int t5;
    int t6;
    int t7;
    int t8;
    int delay_between_measurements;
};
struct Rigol_config_impulse{
    int gain;
    double start_freq;   // в Гц
    double stop_freq;    // в Гц
    double freq_step;    // в Гц
};
class ConfigParser{
    public:
        ConfigParser(const std::string& fname);
        CV_config parse_cv_config();
        Rabi_config parse_rabi_config();
        Impulse_config parse_impulse_config();
        Rigol_config_cv parse_rigol_config_cv();
        Rigol_config_impulse parse_rigol_config_impulse();
        Rigol_config_rabi parse_rigol_config_rabi();
    private:
        std::ifstream config_file;
        double evaluate_expression(const std::string& expr);

};