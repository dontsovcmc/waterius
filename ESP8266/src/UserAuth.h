class UserAuth
{
    public:
    /**
     * Sends JSON POST {email,token}
     * true IF auth success OR user is already authed
     **/
    static bool login(char *email, char *token);
    
    /**
     * Sends JSON POST {email,token}
     * true IF reg success
     **/
    static bool reg(char *email);
};
