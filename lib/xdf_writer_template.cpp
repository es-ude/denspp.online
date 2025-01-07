#include "xdf_writer_template.h"
#include <iostream>


void write_header(XDFWriter* writer, const Config& cfg ) {
    std::ostringstream xml;
    xml << "<?xml version=\"1.0\"?>"
        << "<info>"
        << "<name>SaveUtahData</name>"
        << "<type>EEG</type>"
        << "<channel_count>" << cfg.n_channel << "</channel_count>"
        << "<nominal_srate>" << cfg.sampling_rate << "</nominal_srate>"
        << "<channel_format>" << "double64" << "</channel_format>"
        << "<created_at>50942.723319709003</created_at>"
        << "</info>";

    // Convert to string
    std::string content = xml.str();
    writer->write_stream_header(0,content);
}
void write_footer(XDFWriter* writer,const Config& cfg ,double exact_ts, long time_stamp) {
    std::cout << "Finished Recording all Samples" << std::endl;
    std::cout << "Final Timestamp: " << exact_ts << std::endl;
    std::cout << "Final Sample Count: "<< time_stamp <<std::endl;

    std::ostringstream xml;
    xml << "<?xml version=\"1.0\"?>"
        << "<info>"
        << "<first_timestamp>0.0</first_timestamp>"
        << "<last_timestamp>" << cfg.recording.duration << "</last_timestamp>"
        << "<sample_count>" << cfg.recording.duration * cfg.sampling_rate << "</sample_count>"
        << "<clock_offsets>"
        << "<offset><time>0</time><value>0</value></offset>"
        << "</clock_offsets>"
        << "</info>";

    std::string footer = xml.str();
    writer->write_boundary_chunk();
    writer->write_stream_footer(0, footer);
}
