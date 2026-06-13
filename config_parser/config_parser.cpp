#include <inipp/inipp.h>
#include <fstream>
#include <iostream>
#include <string>
#include "config_parser.hpp"

ConfigParser::ConfigParser(const std::string& fname) {
    // Открываем файл
    std::ifstream conf_file(fname);
    if (!conf_file.is_open()) {
        throw std::runtime_error("Cannot open" + fname);
    }
    config_file = std::move(conf_file);
}
CV_config ConfigParser::parse_cv_config() {
    // Создаем объект парсера
    CV_config ret;
    inipp::Ini<char> ini;    
    // Парсим (false означает "не перезаписывать существующие секции")
    ini.parse(config_file);
    
    
    // Метод extract возвращает true, если значение найдено и успешно преобразовано
    if (!inipp::extract(ini.sections["General"]["number_of_repeats"], ret.num_repeats)) {
        std::cerr << "Failed to read number_of_repeats, using default" << std::endl;
    }
    
    inipp::extract(ini.sections["General"]["t1"], ret.t1);
    inipp::extract(ini.sections["General"]["t2"], ret.t2);
    inipp::extract(ini.sections["General"]["t4"], ret.t4);
    inipp::extract(ini.sections["General"]["t5"], ret.t5);
    
    // Проверяем результат
    std::cout << "number_of_repeats = " << ret.num_repeats << std::endl;
    std::cout << "t1 = " << ret.t1 << " ns" << std::endl;
    std::cout << "t2 = " << ret.t2 << " ns" << std::endl;
    std::cout << "t3 = " << ret.t4 << " ns" << std::endl;
    std::cout << "t4 = " << ret.t5 << " ns" << std::endl;
    
    // Альтернативный способ обхода всех ключей в секции
  
    
    return ret;
}
Rabi_config ConfigParser::parse_rabi_config() {
    // Создаем объект конфига
    Rabi_config ret;
    inipp::Ini<char> ini;
    
    // Парсим файл
    ini.parse(config_file);
    
    // Извлекаем значения из секции General
    if (!inipp::extract(ini.sections["General"]["N"], ret.N)) {
        std::cerr << "Failed to read N, using default" << std::endl;
    }
    
    if (!inipp::extract(ini.sections["General"]["t1"], ret.t1)) {
        std::cerr << "Failed to read t1, using default" << std::endl;
    }
    
    if (!inipp::extract(ini.sections["General"]["t2_start"], ret.t2_start)) {
        std::cerr << "Failed to read t2_start, using default" << std::endl;
    }
    
    if (!inipp::extract(ini.sections["General"]["t2_end"], ret.t2_end)) {
        std::cerr << "Failed to read t2_end, using default" << std::endl;
    }
    
    if (!inipp::extract(ini.sections["General"]["t2_step"], ret.t2_step)) {
        std::cerr << "Failed to read t2_step, using default" << std::endl;
    }
    
    if (!inipp::extract(ini.sections["General"]["t3"], ret.t3)) {
        std::cerr << "Failed to read t3, using default" << std::endl;
    }
    
    if (!inipp::extract(ini.sections["General"]["t4"], ret.t4)) {
        std::cerr << "Failed to read t4, using default" << std::endl;
    }
    
    if (!inipp::extract(ini.sections["General"]["t5"], ret.t5)) {
        std::cerr << "Failed to read t5, using default" << std::endl;
    }
    
    if (!inipp::extract(ini.sections["General"]["t6"], ret.t6)) {
        std::cerr << "Failed to read t6, using default" << std::endl;
    }
    
    if (!inipp::extract(ini.sections["General"]["t7"], ret.t7)) {
        std::cerr << "Failed to read t7, using default" << std::endl;
    }
    
    if (!inipp::extract(ini.sections["General"]["delay_between_measurements"], ret.delay_between_measurements)) {
        std::cerr << "Failed to read delay_between_measurements, using default" << std::endl;
    }
    
    // Проверяем результат
    std::cout << "N = " << ret.N << std::endl;
    std::cout << "t1 = " << ret.t1 << " ns" << std::endl;
    std::cout << "t2_start = " << ret.t2_start << " ns" << std::endl;
    std::cout << "t2_end = " << ret.t2_end << " ns" << std::endl;
    std::cout << "t2_step = " << ret.t2_step << " ns" << std::endl;
    std::cout << "t3 = " << ret.t3 << " ns" << std::endl;
    std::cout << "t4 = " << ret.t4 << " ns" << std::endl;
    std::cout << "t5 = " << ret.t5 << " ns" << std::endl;
    std::cout << "t6 = " << ret.t6 << " ns" << std::endl;
    std::cout << "t7 = " << ret.t7 << " ns" << std::endl;
    std::cout << "delay_between_measurements = " << ret.delay_between_measurements << " ns" << std::endl;
    
    return ret;
}
Impulse_config ConfigParser::parse_impulse_config() {
    // Создаем объект конфига
    Impulse_config ret;
    inipp::Ini<char> ini;
    
    // Парсим файл
    ini.parse(config_file);
    
    // Извлекаем значения из секции General
    if (!inipp::extract(ini.sections["General"]["N"], ret.N)) {
        std::cerr << "Failed to read N, using default" << std::endl;
    }
    
    if (!inipp::extract(ini.sections["General"]["t1"], ret.t1)) {
        std::cerr << "Failed to read t1, using default" << std::endl;
    }
    
    if (!inipp::extract(ini.sections["General"]["t2_start"], ret.t2_start)) {
        std::cerr << "Failed to read t2_start, using default" << std::endl;
    }
    
    if (!inipp::extract(ini.sections["General"]["t2_end"], ret.t2_end)) {
        std::cerr << "Failed to read t2_end, using default" << std::endl;
    }
    
    if (!inipp::extract(ini.sections["General"]["t2_step"], ret.t2_step)) {
        std::cerr << "Failed to read t2_step, using default" << std::endl;
    }
    
    if (!inipp::extract(ini.sections["General"]["t3"], ret.t3)) {
        std::cerr << "Failed to read t3, using default" << std::endl;
    }
    
    if (!inipp::extract(ini.sections["General"]["t4"], ret.t4)) {
        std::cerr << "Failed to read t4, using default" << std::endl;
    }
    
    if (!inipp::extract(ini.sections["General"]["t5"], ret.t5)) {
        std::cerr << "Failed to read t5, using default" << std::endl;
    }
    
    if (!inipp::extract(ini.sections["General"]["t6"], ret.t6)) {
        std::cerr << "Failed to read t6, using default" << std::endl;
    }
    
    if (!inipp::extract(ini.sections["General"]["t7"], ret.t7)) {
        std::cerr << "Failed to read t7, using default" << std::endl;
    }
    if (!inipp::extract(ini.sections["General"]["t8"], ret.t8)) {
        std::cerr << "Failed to read t8, using default" << std::endl;
    }
    if (!inipp::extract(ini.sections["General"]["delay_between_measurements"], ret.delay_between_measurements)) {
        std::cerr << "Failed to read delay_between_measurements, using default" << std::endl;
    }
    
    // Проверяем результат
    std::cout << "N = " << ret.N << std::endl;
    std::cout << "t1 = " << ret.t1 << " ns" << std::endl;
    std::cout << "t2_start = " << ret.t2_start << " ns" << std::endl;
    std::cout << "t2_end = " << ret.t2_end << " ns" << std::endl;
    std::cout << "t2_step = " << ret.t2_step << " ns" << std::endl;
    std::cout << "t3 = " << ret.t3 << " ns" << std::endl;
    std::cout << "t4 = " << ret.t4 << " ns" << std::endl;
    std::cout << "t5 = " << ret.t5 << " ns" << std::endl;
    std::cout << "t6 = " << ret.t6 << " ns" << std::endl;
    std::cout << "t7 = " << ret.t7 << " ns" << std::endl;
    std::cout << "t7 = " << ret.t8 << " ns" << std::endl;
    std::cout << "delay_between_measurements = " << ret.delay_between_measurements << " ns" << std::endl;
    
    return ret;
}
double ConfigParser::evaluate_expression(const std::string& expr) {
    std::istringstream iss(expr);
    double val1, val2;
    char op;
    
    // Пробуем прочитать как "число оператор число"
    if (iss >> val1 >> op >> val2) {
        if (op == '*') return val1 * val2;
        if (op == '/') return val1 / val2;
        if (op == '+') return val1 + val2;
        if (op == '-') return val1 - val2;
    }
    
    // Если не выражение — пробуем как простое число
    iss.clear();
    iss.seekg(0);
    if (iss >> val1) {
        return val1;
    }
    
    // Ничего не распарсили
    std::cerr << "Warning: Cannot evaluate expression '" << expr << "', using 0.0\n";
    return 0.0;
}

Rigol_config_cv ConfigParser::parse_rigol_config_cv() {
    inipp::Ini<char> ini;
    Rigol_config_cv config{};
    
    // Значения по умолчанию
    config.gain = 1;
    config.start_freq = 0.0;
    config.stop_freq = 0.0;
    config.freq_step = 0.0;
    
    if (!config_file.is_open()) {
        std::cerr << "Config file is not open. Returning default Rigol config.\n";
        return config;
    }
    
    // Сбрасываем указатель чтения файла на начало
    config_file.clear();
    config_file.seekg(0);
    
    // Парсим ini-файл
    ini.parse(config_file);
    
    // Извлекаем gain — обычное целое число
    std::string gain_str = ini.sections["Rigol"]["gain"];
    if (!gain_str.empty()) {
        try {
            config.gain = std::stoi(gain_str);
        } catch (...) {
            std::cerr << "Warning: Invalid gain value, using default\n";
        }
    }
    
    // Извлекаем частоты и вычисляем выражения
    std::string start_str = ini.sections["Rigol"]["start_freq"];
    if (!start_str.empty()) {
        config.start_freq = evaluate_expression(start_str);
        
    }
    
    std::string stop_str = ini.sections["Rigol"]["stop_freq"];
    if (!stop_str.empty()) {
        config.stop_freq = evaluate_expression(stop_str);
    }
    
    std::string step_str = ini.sections["Rigol"]["freq_step"];
    if (!step_str.empty()) {
        config.freq_step = evaluate_expression(step_str);
    }
    
    return config;
}
Rigol_config_rabi ConfigParser::parse_rigol_config_rabi() {
    inipp::Ini<char> ini;
    Rigol_config_rabi config{};
    
    // Значения по умолчанию
    config.gain = 1;
    config.freq = 0.0;

    
    if (!config_file.is_open()) {
        std::cerr << "Config file is not open. Returning default Rigol config.\n";
        return config;
    }
    
    // Сбрасываем указатель чтения файла на начало
    config_file.clear();
    config_file.seekg(0);
    
    // Парсим ini-файл
    ini.parse(config_file);
    
    // Извлекаем gain — обычное целое число
    std::string gain_str = ini.sections["Rigol"]["gain"];
    if (!gain_str.empty()) {
        try {
            config.gain = std::stoi(gain_str);
        } catch (...) {
            std::cerr << "Warning: Invalid gain value, using default\n";
        }
    }
    
    // Извлекаем частоты и вычисляем выражения
    std::string start_str = ini.sections["Rigol"]["freq"];
    if (!start_str.empty()) {
        config.freq = evaluate_expression(start_str);
        
    }
    
    return config;
}
Rigol_config_impulse ConfigParser::parse_rigol_config_impulse() {
    inipp::Ini<char> ini;
    Rigol_config_impulse config{};
    
    // Значения по умолчанию
    config.gain = 1;
    config.start_freq = 0.0;
    config.stop_freq = 0.0;
    config.freq_step = 0.0;
    
    if (!config_file.is_open()) {
        std::cerr << "Config file is not open. Returning default Rigol config.\n";
        return config;
    }
    
    // Сбрасываем указатель чтения файла на начало
    config_file.clear();
    config_file.seekg(0);
    
    // Парсим ini-файл
    ini.parse(config_file);
    
    // Извлекаем gain — обычное целое число
    std::string gain_str = ini.sections["Rigol"]["gain"];
    if (!gain_str.empty()) {
        try {
            config.gain = std::stoi(gain_str);
        } catch (...) {
            std::cerr << "Warning: Invalid gain value, using default\n";
        }
    }
    
    // Извлекаем частоты и вычисляем выражения
    std::string start_str = ini.sections["Rigol"]["start_freq"];
    if (!start_str.empty()) {
        config.start_freq = evaluate_expression(start_str);
        
    }
    
    std::string stop_str = ini.sections["Rigol"]["stop_freq"];
    if (!stop_str.empty()) {
        config.stop_freq = evaluate_expression(stop_str);
    }
    
    std::string step_str = ini.sections["Rigol"]["freq_step"];
    if (!step_str.empty()) {
        config.freq_step = evaluate_expression(step_str);
    }
    
    return config;
}