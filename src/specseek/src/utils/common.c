/*
    For backwards Compatability between many operating systems including
    Hobby os's, some may not be as far along as others, as a result I will try
    to fill in some of the more simple functions that may not be implemented in your
    clib, however I cannot do everything

    mell xoxo
*/

/// @brief checks if a char is a digit
/// @param c target
/// @return int 0 or 1 
unsigned int isadigit(char c){
    return (unsigned)(c - '0') <= 9;
}