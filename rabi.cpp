#include <Python.h>
#include "packet_collector/fpga2.h"
#include <inipp/inipp.h>
#include "config_parser.hpp"
#include "RigolDriver.hpp"
#include <thread>
#include <iostream>
#include <signal.h>
int setupSpincore(Rabi_config conf) {
    PyObject* pModule = PyImport_ImportModule("builder");
    if (!pModule) {
        PyErr_Print();
        printf("ERROR: Failed to import builder\n");
        return 1;
    }
    
    PyObject* pFunc = PyObject_GetAttrString(pModule, "build_impulses_rabi");
    if (!pFunc || !PyCallable_Check(pFunc)) {
        PyErr_Print();
        printf("ERROR: Function 'build_impulses_rabi' not found or not callable\n");
        return 1;
    }
    PyObject* pArgs = PyTuple_New(11);
    // Create arguments tuple
    PyTuple_SetItem(pArgs, 0, PyLong_FromLong(conf.t1));
    PyTuple_SetItem(pArgs, 1, PyLong_FromLong(conf.t2_start));
    PyTuple_SetItem(pArgs, 2, PyLong_FromLong(conf.t2_end));
    PyTuple_SetItem(pArgs, 3, PyLong_FromLong(conf.t2_step));
    PyTuple_SetItem(pArgs, 4, PyLong_FromLong(conf.t3));
    PyTuple_SetItem(pArgs, 5, PyLong_FromLong(conf.t4));
    PyTuple_SetItem(pArgs, 6, PyLong_FromLong(conf.t5));
    PyTuple_SetItem(pArgs, 7, PyLong_FromLong(conf.t6));
    PyTuple_SetItem(pArgs, 8, PyLong_FromLong(conf.t7));
    PyTuple_SetItem(pArgs, 9, PyLong_FromLong(conf.N));
    PyTuple_SetItem(pArgs, 10, PyLong_FromLong(conf.delay_between_measurements));
    
    
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
    auto c = ConfigParser("rabi.ini");
    auto conf = c.parse_rabi_config();

    Rigol_config_rabi rigol = c.parse_rigol_config_rabi();
    
    std::cout << "Rigol configuration:\n";
    std::cout << "  gain = " << rigol.gain << std::endl;
    std::cout << "  start_freq = " << rigol.freq / 1e6 << " MHz\n";

    //auto r = RigolDriver();
    //r.setup_rabi(rigol.gain,rigol.freq);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    // Initialize Python once
    Py_Initialize();
    PyRun_SimpleString("import sys");
    PyRun_SimpleString("sys.path.append('.')");
    PyRun_SimpleString("sys.path.append('./spincore_driver')");
    auto points = (conf.t2_end-conf.t2_start) / conf.t2_step;
    std::cout<<"pts:"<<points<<std::endl;
    setupSpincore(conf);  
    long long unsigned total_time_us = (conf.t1/1000 + (conf.t2_start+(points/2+1)*conf.t2_step)/1000 + conf.t3/1000 + conf.t4/1000+ conf.t5/1000+ conf.t6/1000+ conf.t7/1000 + conf.delay_between_measurements/1000);
    long long unsigned total_time_ms = static_cast<unsigned long long>((static_cast<double>(total_time_us) / 1000.0) * static_cast<unsigned long long>(conf.N) * static_cast<unsigned long long>(points)); 
    std::cout << "Waiting " << total_time_ms +26000<< " ms..." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(total_time_ms));
    
    // Даём время на корректное завершение
    std::this_thread::sleep_for(std::chrono::milliseconds(26000));
    // Отправляем SIGINT для остановки захвата
    kill(getpid(), SIGINT);
    Py_Finalize();
    
    return 0;
}