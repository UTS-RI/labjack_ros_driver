#include "labjackt7test/labjack_t7_driver.h"

labjack_t7_driver::labjack_t7_driver(int chan_num,int acq_rate,bool verbose,std::string identifier,double serial_num,int dev_type,int comm_type)
{
    // input user chosen parameters
    _def_param._serial_number = serial_num;
    _def_param._device_type = dev_type;
    _def_param._comms_type = comm_type;
    _def_param._verbose = verbose;
    _def_param._num_channel = chan_num;
    _def_param._acqrate = acq_rate;
    _def_param._identifier = identifier;

    // set boolean flags to default value
    _def_param._dev_found = openConnection();
    _def_param._streaming = false;

    _acquisition = false;


    // set address list for acquisition assuming conventional use
    // (if 4 channels, uses AIN0-3, if 8 channels, uses AIN0-7)
    if(_def_param._verbose)
    {
        std::cout << "Address list: ";
    }
    for (int i=0;i<_def_param._num_channel*2;i+=2)
    {
        _addresses.push_back(i);
        if(_def_param._verbose)
        {
            std::cout << i << ", ";
        }
    }
    if(_def_param._verbose)
    {
        std::cout << std::endl;
    }
}

labjack_t7_driver::~labjack_t7_driver()
{
    if(!_def_param._dev_found)
    {
        return;
    }

    // force stop acquisition if streaming
    if(_def_param._streaming && _acquisition)
    {
        stopStream();   // stops stream
        int numtemp = _def_param._num_channel*_str_param._scan_rate/2;
        double temp[numtemp];
        acquireReadings(temp);  // clears buffer
        if(_def_param._verbose)
        {
            std::cout << "Final readings: ";
            for(int i=0;i<numtemp;i++)
            {
                std::cout << temp[i] << ", ";
            }
        }
    }

    // force close connection
    do{
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        closeConnection();
        if(checkConnection())
        {
            if(_def_param._verbose)
            {
                std::cout << "Connection still exist. Trying closing connection in 500ms." << std::endl;
            }
        }
    }
    while(checkConnection());
}

void labjack_t7_driver::setAddressList(std::vector<int> add_list)
{
    _addresses.clear();
    _addresses = add_list;
}

double labjack_t7_driver::getSerialNumber()
{
    if(!_def_param._dev_found)
    {
        if(_def_param._verbose)
        {
            std::cout << "No device connected." << std::endl;
        }
        return 0;
    }
    else
    {
        return _def_param._serial_number;
    }
}

bool labjack_t7_driver::checkConnection()
{
    return _def_param._dev_found;
}

bool labjack_t7_driver::checkStreaming()
{
    return _def_param._streaming;
}


// default functionality
std::vector<double> labjack_t7_driver::getCurrentReadings()
{
    std::vector<double> temp;
    double tempval;
    for(std::vector<int>::iterator it=_addresses.begin();it!=_addresses.end();it++)
    {
        _def_param._err_code = LJM_eReadAddress(_def_param._device_handle,*it,LJM_INT32,&tempval);
        temp.push_back(tempval);
    }
    return temp;
}


// streaming functionality
void labjack_t7_driver::setStreaming()
{
    if(_def_param._streaming)
    {
        if(_def_param._verbose)
        {
            std::cout << "Driver already set to stream data." << std::endl;
        }
        return;
    }
    _def_param._streaming = true;
    if(_def_param._verbose)
    {
        std::cout << "Setting driver to stream data from addresses: ";
        for(std::vector<int>::iterator it=_addresses.begin();it!=_addresses.end();it++)
        {
            std::cout << *it << ", ";
        }
        std::cout << std::endl << "Driver streaming scan rate: " << _str_param._scan_rate << " Hz" << std::endl;
    }
}

void labjack_t7_driver::unsetStreaming()
{
    if(!_def_param._streaming)
    {
        if(_def_param._verbose)
        {
            std::cout << "Driver already in default functionality." << std::endl;
        }
        return;
    }
    _def_param._streaming = false;
    if(_def_param._verbose)
    {
        std::cout << "Unset driver to stream data from device. Default functionality is now on." << std::endl;
    }
}

void labjack_t7_driver::setScanRate(double scanrate)
{
    _str_param._scan_rate = scanrate;
    _str_param._scans_per_read = scanrate/_def_param._num_channel;
}

double labjack_t7_driver::getScanRate()
{
    return _str_param._scan_rate;
}

int labjack_t7_driver::getDataSize()
{
    return _str_param._scans_per_read*_def_param._num_channel;
}

bool labjack_t7_driver::startStream()
{
    if(_acquisition)
    {
        if(_def_param._verbose)
        {
            std::cout << "Stream already started." << std::endl;
        }
        return false;
    }

    _acquisition = true;
    int scanList[_def_param._num_channel];
    std::copy(_addresses.begin(),_addresses.end(),scanList);
    if(_def_param._verbose)
    {
        std::cout << "Device Handle: " << _def_param._device_handle << "; Scan per read: " << _str_param._scans_per_read << "; Size of addresses: " << _addresses.size() << "; Scan rate: " << _str_param._scan_rate << std::endl;
        std::cout << "Setting streaming settings as per error code 2942 - for USB connection." << std::endl;
    }
    // sets stream settling us to 0
    _def_param._err_code = LJM_eWriteAddress(_def_param._device_handle,4008,LJM_FLOAT32,0);
    if(_def_param._verbose)
    {
        std::cout << "Set stream settling us to 0. Error code: " << _def_param._err_code << std::endl;
    }
    // sets stream resolution index to 0
    _def_param._err_code = LJM_eWriteAddress(_def_param._device_handle,4010,LJM_UINT32,0);
    if(_def_param._verbose)
    {
        std::cout << "Set stream resolution index to 0. Error code: " << _def_param._err_code << std::endl;
    }

    _def_param._err_code = LJM_eStreamStart(_def_param._device_handle,_str_param._scans_per_read,_addresses.size(),scanList,&_str_param._scan_rate);
    if(_def_param._err_code != 0)
    {
        _acquisition = false;
        if(_def_param._verbose)
        {
            std::cout << "Error in starting stream. Stream not started. Error code: " << _def_param._err_code << std::endl;
        }
        return false;
    }
    return true;
}

void labjack_t7_driver::stopStream()
{
    if(!_acquisition)
    {
        if(_def_param._verbose)
        {
            std::cout << "Stream already stopped for device." << std::endl;
        }
        return;
    }
    _def_param._err_code = LJM_eStreamStop(_def_param._device_handle);
    _acquisition = false;
    if(_def_param._err_code != 0)
    {
        if(_def_param._verbose)
        {
            std::cout << "Error in stopping stream. Error code: " << _def_param._err_code << std::endl;
        }
        _acquisition = true;
    }
    else
    {
        if(_def_param._verbose)
        {
            std::cout << "Stream stopped for device serial number: " << getSerialNumber() << std::endl;
        }
    }
}

bool labjack_t7_driver::acquireReadings(double *data)
{
    if(!_acquisition)
    {
        if(_def_param._verbose)
        {
            if(!_def_param._streaming)
            {
                std::cout << "Streaming functionality not set for driver." << std::endl;
            }
            else
            {
                std::cout << "Can't acquire readings. No stream has been started." << std::endl;
            }
        }
        return false;
    }
    _def_param._err_code = LJM_eStreamRead(_def_param._device_handle,data,&_str_param._devscan_backlog,&_str_param._ljmscan_backlog);
    if(_def_param._err_code != 0)
    {
        if(_def_param._verbose)
        {
            std::cout << "Error in stream read. Error code " << _def_param._err_code << std::endl;
        }
        return false;
    }
    else
    {
        if(_def_param._verbose)
        {
            std::cout << "Stream read. Error code: " << _def_param._err_code << "; Device backlog: " << _str_param._devscan_backlog << "; LJM backlog: " << _str_param._ljmscan_backlog << std::endl;
        }
        return true;
    }
}


// Private functions
bool labjack_t7_driver::openConnection()
{
    try
    {
        _def_param._err_code = LJM_Open(_def_param._device_type,_def_param._comms_type,_def_param._identifier.c_str(),&_def_param._device_handle);        
    }
    catch(const std::exception& e)
    {
        if(_def_param._verbose)
        {
            std::cerr << e.what() << '\n';
        }
    }
    if(_def_param._err_code !=0)
    {
        if(_def_param._verbose)
        {
            std::cout << "Error in opening connection with device." << std::endl;
        }
        return false;
    }

    try{
        if(_def_param._verbose)
        {
            std::cout << "Reading serial number" << std::endl;
        }
        _def_param._err_code = LJM_eReadName(_def_param._device_handle,"SERIAL_NUMBER",&_def_param._serial_number);
        if(_def_param._err_code != 0)
        {
            if(_def_param._verbose)
            {
                std::cout << "Error in reading serial number!" << std::endl;
            }
            LJM_CloseAll();
            return false;
        }        
    }
    catch(const std::exception& ex)
    {
        if(_def_param._verbose)
        {
            std::cerr << ex.what() << std::endl;
        }
        LJM_CloseAll();
        return false;
    }
    return true;
}

void labjack_t7_driver::closeConnection()
{
    if(!_def_param._dev_found)
    {
        return;
    }

    if(_def_param._verbose)
    {
        std::cout << "Closing connection to device serial number: " << _def_param._serial_number << std::endl;
    }
    _def_param._err_code = LJM_Close(_def_param._device_handle);
    if(_def_param._err_code !=0 && _def_param._verbose)
    {
        std::cout << "Error in closing connection with device" << std::endl;
    }
    _def_param._dev_found = false;
}