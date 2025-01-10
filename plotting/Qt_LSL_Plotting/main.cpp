#include "mainwindow.h"
#include <iostream>
#include <cstdio>
#include <QApplication>
#include <fstream>
#include <sstream>
#include "../../lib/config.h"

std::vector<std::vector<int>> generateDefaultMapping(const int n_channel){
    std::vector<std::vector<int>> mapping;
    int col = static_cast<int>(std::sqrt(n_channel));
        if (col * col < n_channel) {
            col += 1;
    }
    int row = (n_channel + col - 1) / col;
    std::cout << "Grid dimensions: " << row << " x " << col << std::endl;
    for (int j = 0; j < row; j++) {
        std::vector<int> row_vec(col, 0);
        for (int i = 0; i < col; i++) {
            int value = j * col + i + 1;
                if (value <= n_channel) {
                   row_vec[i] = value;
                }
            }
       mapping.emplace_back(row_vec);
    }
    return mapping;
}

std::vector<std::vector<int>> readCSV(const std::string& filePath, int n_channel) {
    std::vector<std::vector<int>> data;
    std::ifstream file(filePath);

    if (!file.is_open()) {
        std::cerr << "Error: Could not open file " << filePath << ". Generating default mapping..."<< std::endl;
        data = generateDefaultMapping(n_channel);
        return data;
    }

    std::string line;
    while (std::getline(file, line)) {
        std::vector<int> row;
        std::stringstream ss(line);
        std::string value;

        while (std::getline(ss, value, ';')) {
            row.push_back(std::stoi(value));
        }

        data.push_back(row);
    }

    file.close();
    return data;
}





int main(int argc, char *argv[])
{
    std::string config_file_path = (argc >= 2) ? argv[1] : "config/default.yaml";

    Config cfg = readConfig(config_file_path);

    std::vector<std::vector<int>> mapping;

    if(cfg.use_layout == true and cfg.mapping_path != "default"){
        std::cout << "Trying to generate Custom Mapping..."<< std::endl;
        std::string layout_path = cfg.mapping_path;
        mapping = readCSV(layout_path, cfg.n_channel);
    }else{
        std::cout << "Generating Default Mapping..." << std::endl;
        mapping = generateDefaultMapping(cfg.n_channel);
    }

    for (const auto& row : mapping) {
        for (const auto& val : row) {
            std::cout << val << " ";
        }
        std::cout << std::endl;
    }

    QApplication a(argc, argv);
    MainWindow w(0,0, mapping);
    w.show();
    return a.exec();
}
