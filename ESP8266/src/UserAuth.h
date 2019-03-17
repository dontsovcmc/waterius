class UserAuth
{
    public:
    /**
     * Sends JSON POST {email,token}
     * true IF login success
     **/
    static bool login(char *email, char *token);
};
