#include "setup.h"
#include "master_i2c.h"

class UserClass
{
    public:
    /**
     * Sends new data to server
     * true IF response code = 200
     **/
    static bool sendNewData(const Settings &settings, const SlaveData &data, const float &channel0, const float &channel1);
};
