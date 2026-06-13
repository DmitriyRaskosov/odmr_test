#include <Python.h>
#include "packet_collector/fpga2.h"
#include <inipp/inipp.h>
#include "config_parser.hpp"
#include "RigolDriver.hpp"
#include <thread>
#include <iostream>
#include <signal.h>
int setupSpincore(CV_config conf, int points) {

    
    PyObject* pModule = PyImport_ImportModule("builder");
    if (!pModule) {
        PyErr_Print();
        printf("ERROR: Failed to import builder\n");
        return 1;
    }
    
    PyObject* pFunc = PyObject_GetAttrString(pModule, "build_impulses_for_cv_odmr");
    if (!pFunc || !PyCallable_Check(pFunc)) {
        PyErr_Print();
        printf("ERROR: Function 'build_impulses_for_cv_odmr' not found or not callable\n");
        return 1;
    }
    
    // Create arguments tuple
    PyObject* pArgs = PyTuple_New(6);
    PyTuple_SetItem(pArgs, 0, PyLong_FromLong(points));
    PyTuple_SetItem(pArgs, 1, PyLong_FromLong(conf.t1));
    PyTuple_SetItem(pArgs, 2, PyLong_FromLong(conf.t2));
    PyTuple_SetItem(pArgs, 3, PyLong_FromLong(conf.t4));
    PyTuple_SetItem(pArgs, 4, PyLong_FromLong(conf.t5));
    PyTuple_SetItem(pArgs, 5, PyLong_FromLong(conf.num_repeats));
    
    PyObject* pValue = PyObject_CallObject(pFunc, pArgs);
    
    if (pValue) {
        long result = PyLong_AsLong(pValue);
        printf("The result from Python is: %ld\n", result);
        Py_DECREF(pValue);
    } else {
        PyErr_Print();
    }

    // Clean up
    Py_DECREF(pArgs);
    Py_DECREF(pFunc);
    Py_DECREF(pModule);
    
    // Remove Py_Finalize() - Python will be finalized in main()
    return 0;
}
int main(int argc, char **argv){
    std::thread t1(start_capture,argc,argv);
    t1.detach();
    auto c = ConfigParser("cv_odmr.ini");
    auto conf = c.parse_cv_config();

    Rigol_config_cv rigol = c.parse_rigol_config_cv();
    
    std::cout << "Rigol configuration:\n";
    std::cout << "  gain = " << rigol.gain << std::endl;
    std::cout << "  start_freq = " << rigol.start_freq / 1e6 << " MHz\n";
    std::cout << "  stop_freq = " << rigol.stop_freq / 1e6 << " MHz\n";
    std::cout << "  freq_step = " << rigol.freq_step / 1e3 << " kHz\n";
    auto points = static_cast<int>((rigol.stop_freq-rigol.start_freq)/rigol.freq_step);
    auto r = RigolDriver();
    r.setup_sweep_for_imp_odmr(rigol.gain,rigol.start_freq,rigol.stop_freq,rigol.freq_step);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    // Initialize Python once
    Py_Initialize();
    PyRun_SimpleString("import sys");
    PyRun_SimpleString("sys.path.append('.')");
    PyRun_SimpleString("sys.path.append('./spincore_driver')");
    
    setupSpincore(conf,points);  
    long long unsigned total_time_us = points * (conf.t1/1000 + conf.t2/1000 + conf.t4/1000 + conf.t5/1000) * conf.num_repeats;
    long long unsigned total_time_ms = total_time_us / 1000 + 2*conf.num_repeats*points; 
    std::cout << "Waiting " << total_time_ms +26000<< " ms..." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(total_time_ms));
    
    // Даём время на корректное завершение
    std::this_thread::sleep_for(std::chrono::milliseconds(26000));
    // Отправляем SIGINT для остановки захвата
    kill(getpid(), SIGINT);
    Py_Finalize();
    
    return 0;
}