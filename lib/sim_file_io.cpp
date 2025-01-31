#include "sim_file_io.h"

#include <stdexcept>
#include <algorithm>

bool checkFileType(const std::string& filepath) {
    // true: file ends on .xdf, false: file ends on .mat

    size_t dotPos = filepath.rfind('.');
    if (dotPos == std::string::npos) {
        throw std::runtime_error("No dot found :c"); //
    }
    std::string extension = filepath.substr(dotPos + 1);
    // Convert to lowercase for case-insensitive comparison
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

    if (extension == "mat") {
        return false;
    } else if (extension == "xdf") {
        return true;
    } else {
        throw std::runtime_error("Unsupported file extension");
    }
}

matvar_t* handle_mat_file(mat_t* matfile) {
    if (!matfile) {
        throw std::runtime_error("Could not open .mat file");
    }

    matvar_t *raw_data = Mat_VarRead(matfile, "rawdata");
    if (!raw_data) {
        throw std::runtime_error("Could not open .mat file");
    }

    matvar_t *spikeVar = Mat_VarGetStructFieldByName(raw_data, "spike", 0);
    if (!spikeVar) {
        throw std::runtime_error("Could not open .mat file");
    }
    return spikeVar;
}