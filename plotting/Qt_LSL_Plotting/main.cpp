#include "mainwindow.h"
#include <iostream>
#include <cstdio>
#include <QApplication>
#include <fstream>
#include <sstream>

std::vector<std::vector<int>> readCSV(const std::string& filePath) {
    std::vector<std::vector<int>> data;
    std::ifstream file(filePath);

    if (!file.is_open()) {
        std::cerr << "Error: Could not open file " << filePath << std::endl;
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
    std::string layout_path = "../../config/layout_mea/Mapping_Utah.csv";
    if (argc >= 2){
        layout_path = std::string(argv[1]);
    }

    std::vector<std::vector<int>> mapping = readCSV(layout_path);

    for (const auto& row : mapping) {
        for (const auto& val : row) {
            std::cout << val << " ";
        }
        std::cout << std::endl;
    }

    std::cout << layout_path << std::endl;
    QApplication a(argc, argv);
    MainWindow w(0,0, mapping);
    w.show();
    return a.exec();
}
