#ifndef REVERSE_HPP
#define REVERSE_HPP


uint16_t revert_w(uint16_t src) {
    char tmp1[2];
    uint16_t dst;
    memcpy(tmp1,&src,2);
    char tmp2[]={tmp1[1],tmp1[0]};
    memcpy(&dst,tmp2,2);
    return dst;
}

int64_t revert_l(int64_t src) {
    char tmp1[8];
    int64_t dst;
    memcpy(tmp1,&src,8);
    char tmp2[]={tmp1[7],tmp1[6],tmp1[5],tmp1[4],tmp1[3],tmp1[2],tmp1[1],tmp1[0]};
    memcpy(&dst,tmp2,8);
    return dst;
}

double revert_d(double src) {
    char tmp1[8];
    double dst;
    memcpy(tmp1,&src,8);
    char tmp2[]={tmp1[7],tmp1[6],tmp1[5],tmp1[4],tmp1[3],tmp1[2],tmp1[1],tmp1[0]};
    memcpy(&dst,tmp2,8);
    return dst;
}


uint32_t revert_32(uint32_t src){
    char tmp1[4];
    uint32_t dst;
    memcpy(tmp1,&src,4);
    char tmp2[] = {tmp1[3],tmp1[2],tmp1[1],tmp1[0]};
    memcpy(&dst,tmp2,4);
    return dst;

}


#endif // REVERSE_HPP
