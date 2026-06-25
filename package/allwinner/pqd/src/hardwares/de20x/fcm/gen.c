
#include <stdio.h>
#include <stdlib.h>

int SATHBH_Protect[13] = {160, 190, 220, 240, 250, 255, 255, 255, 255, 255, 255, 255, 255};
int LUMHBH_Protect[13] = {160, 190, 220, 240, 250, 255, 255, 255, 255, 255, 255, 255, 255};

int SATSBH_Protect[13] = {0, 20, 40, 60, 90, 130, 160, 190, 220, 240, 255, 255, 255};
int LUMSBH_Protect[13] = {0, 40, 80, 120, 150, 180, 200, 220, 230, 245, 255, 255, 255};

int SATYBH_Protect[13] = {0, 10, 25, 50, 110 ,170, 200, 220, 230, 240, 255, 255, 255};
int LUMYBH_Protect[13] = {0, 10, 30, 75, 130 ,170, 210, 230, 240, 250, 255, 255, 255};

void dump(int* data, int len)
{
    for (int i = 0; i < len; i++) {
        float d = data[i] / 255.0;
        printf(" %5.2f,", d);
    }
    printf("\n");
}

void dump_test(int* data, int len, float b, int range)
{
    for (int i = 0; i < len; i++) {
        float d = (data[i] / 255.0) * b * range;
        printf(" %5.2f,", d);
    }
    printf("\n");
}


int main(int argc, char** argv)
{
    dump(SATHBH_Protect, 13);

    dump(SATSBH_Protect, 13);
    dump(LUMSBH_Protect, 13);

    dump(SATYBH_Protect, 13);
    dump(LUMYBH_Protect, 13);

    printf("------------------------------\n");
    dump_test(SATHBH_Protect, 13, 1.00, 127);
    dump_test(SATSBH_Protect, 13, 1.00, 127);
    dump_test(SATYBH_Protect, 13, 1.00, 127);
    printf("------------------------------\n");
    dump_test(LUMHBH_Protect, 13, 0.84, 127);
    dump_test(LUMSBH_Protect, 13, 0.84, 127);
    dump_test(LUMYBH_Protect, 13, 0.84, 127);

    return 0;
}
