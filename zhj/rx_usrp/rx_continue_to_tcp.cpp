#include <sys/socket.h>
#include <arpa/inet.h>
#include <uhd/exception.hpp>
#include <uhd/transport/udp_simple.hpp>
#include <uhd/types/tune_request.hpp>
#include <uhd/usrp/multi_usrp.hpp>
#include <uhd/utils/safe_main.hpp>
#include <uhd/utils/thread.hpp>
#include <boost/format.hpp>
#include <boost/program_options.hpp>
#include <chrono>
#include <complex>
#include <iostream>
#include <thread>

namespace po = boost::program_options;

int UHD_SAFE_MAIN(int argc, char* argv[])
{
    // variables to be set by po 
    std::string args, file, ant, subdev, ref;
    double rate, freq, gain, bw;
    std::string addr, port;

    // setup the program options
    po::options_description desc("Allowed options");
    // clang-format off
    desc.add_options()
        ("help", "help message")
        ("args", po::value<std::string>(&args)->default_value("addr=127.0.0.1"), "multi uhd device address args")
        ("rate", po::value<double>(&rate)->default_value(1e6), "rate of incoming samples")
        ("freq", po::value<double>(&freq)->default_value(1.45e9), "rf center frequency in Hz")
        ("gain", po::value<double>(&gain)->default_value(70), "gain for the RF chain")
        ("ant", po::value<std::string>(&ant), "antenna selection")
        ("subdev", po::value<std::string>(&subdev), "subdevice specification")
        ("bw", po::value<double>(&bw), "analog frontend filter bandwidth in Hz")
        ("port", po::value<std::string>(&port)->default_value("12345"), "server udp port")
        ("addr", po::value<std::string>(&addr)->default_value("192.168.10.100"), "resolvable server address")
        ("ref", po::value<std::string>(&ref)->default_value("internal"), "reference source (internal, external, mimo)")
        ("int-n", "tune USRP with integer-N tuning")
    ;
    // clang-format on
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    // print the help message
    if (vm.count("help")) {
        std::cout << boost::format("UHD RX to UDP %s") % desc << std::endl;
        return ~0;
    }

    // create a usrp device
    std::cout << std::endl;
    std::cout << boost::format("Creating the usrp device with: %s...") % args
              << std::endl;
    uhd::usrp::multi_usrp::sptr usrp = uhd::usrp::multi_usrp::make(args);
    std::cout << boost::format("Using Device: %s") % usrp->get_pp_string() << std::endl;

    // Lock mboard clocks
    if (vm.count("ref")) {
        usrp->set_clock_source(ref);
    }

    // always select the subdevice first, the channel mapping affects the other settings
    if (vm.count("subdev")) {
        usrp->set_rx_subdev_spec(subdev);
    }

    // set the rx sample rate
    std::cout << boost::format("Setting RX Rate: %f Msps...") % (rate / 1e6) << std::endl;
    usrp->set_rx_rate(rate);
    std::cout << boost::format("Actual RX Rate: %f Msps...") % (usrp->get_rx_rate() / 1e6)
              << std::endl
              << std::endl;

    // set the rx center frequency
    std::cout << boost::format("Setting RX Freq: %f MHz...") % (freq / 1e6) << std::endl;
    uhd::tune_request_t tune_request(freq);
    if (vm.count("int-n"))
        tune_request.args = uhd::device_addr_t("mode_n=integer");
    usrp->set_rx_freq(tune_request);
    std::cout << boost::format("Actual RX Freq: %f MHz...") % (usrp->get_rx_freq() / 1e6)
              << std::endl
              << std::endl;

    // set the rx rf gain
    std::cout << boost::format("Setting RX Gain: %f dB...") % gain << std::endl;
    usrp->set_rx_gain(gain);
    std::cout << boost::format("Actual RX Gain: %f dB...") % usrp->get_rx_gain()
              << std::endl
              << std::endl;

    // set the analog frontend filter bandwidth
    if (vm.count("bw")) {
        std::cout << boost::format("Setting RX Bandwidth: %f MHz...") % (bw / 1e6)
                  << std::endl;
        usrp->set_rx_bandwidth(bw);
        std::cout << boost::format("Actual RX Bandwidth: %f MHz...")
                         % (usrp->get_rx_bandwidth() / 1e6)
                  << std::endl
                  << std::endl;
    }

    // set the antenna
    if (vm.count("ant"))
        usrp->set_rx_antenna(ant);

    std::this_thread::sleep_for(std::chrono::seconds(1)); // allow for some setup time

    // Check Ref and LO Lock detect
    std::vector<std::string> sensor_names;
    sensor_names = usrp->get_rx_sensor_names(0);
    if (std::find(sensor_names.begin(), sensor_names.end(), "lo_locked")
        != sensor_names.end()) {
        uhd::sensor_value_t lo_locked = usrp->get_rx_sensor("lo_locked", 0);
        std::cout << boost::format("Checking RX: %s ...") % lo_locked.to_pp_string()
                  << std::endl;
        UHD_ASSERT_THROW(lo_locked.to_bool());
    }
    sensor_names = usrp->get_mboard_sensor_names(0);
    if ((ref == "mimo")
        and (std::find(sensor_names.begin(), sensor_names.end(), "mimo_locked")
                != sensor_names.end())) {
        uhd::sensor_value_t mimo_locked = usrp->get_mboard_sensor("mimo_locked", 0);
        std::cout << boost::format("Checking RX: %s ...") % mimo_locked.to_pp_string()
                  << std::endl;
        UHD_ASSERT_THROW(mimo_locked.to_bool());
    }
    if ((ref == "external")
        and (std::find(sensor_names.begin(), sensor_names.end(), "ref_locked")
                != sensor_names.end())) {
        uhd::sensor_value_t ref_locked = usrp->get_mboard_sensor("ref_locked", 0);
        std::cout << boost::format("Checking RX: %s ...") % ref_locked.to_pp_string()
                  << std::endl;
        UHD_ASSERT_THROW(ref_locked.to_bool());
    }

    // create a receive streamer
    uhd::stream_args_t stream_args("fc32"); // complex floats
    uhd::rx_streamer::sptr rx_stream = usrp->get_rx_stream(stream_args);

    // setup streaming
    uhd::stream_cmd_t stream_cmd(uhd::stream_cmd_t::STREAM_MODE_START_CONTINUOUS);
    stream_cmd.stream_now = true;
    rx_stream->issue_stream_cmd(stream_cmd);

    // loop until total number of samples reached
    uhd::rx_metadata_t md;
    std::vector<std::complex<float>> buff(rx_stream->get_max_num_samps());


    // 创建TCP套接字
    int sockfd = socket(AF_INET, SOCK_STREAM, 0); 
    if (sockfd < 0)
    {
        std::cerr << "Failed to create socket" << std::endl;
        return 1;
    }

    struct sockaddr_in server_addr
    {
    };
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(12345);                       // 设置目标主机的端口号
    server_addr.sin_addr.s_addr = inet_addr("192.168.10.100"); // 设置目标主机的IP地址

    // 连接到目标主机
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        std::cerr << "Failed to connect to server" << std::endl;
        close(sockfd);
        return 1;
    }

    while (true) {
        size_t num_rx_samps = rx_stream->recv(&buff.front(), buff.size(), md);
            // 将接收到的数据发送到目标主机
        ssize_t num_bytes_sent = send(sockfd, buff.data(), buff.size() * sizeof(std::complex<float>), 0);
    }

    // finished
    std::cout << std::endl << "Done!" << std::endl << std::endl;

    return EXIT_SUCCESS;
}
* text=auto eol=lf
