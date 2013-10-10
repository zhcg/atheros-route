#include<stdio.h>

int main(int argc, char **argv)
{
    FILE *fpr = NULL,*fpr_dst=NULL;
    int value=0;
    int sum=0;
    int num;
    int checksum=0;

    if(argc!=3)
    {
        printf("Usage: calc_otp_checksum filename file_add_checksum\n");
    }

    fpr=fopen(argv[1],"rb");
    fpr_dst=fopen(argv[2],"wb");

    if(fpr==NULL|fpr_dst==NULL)
    {
        printf("file open %s or %s error!\n",argv[1],argv[2]);
        return;
    }

    while((num =fread(&value,4,1,fpr)) == 1) 
    {
       sum = sum ^ value;
       fwrite(&value,4,1,fpr_dst);
    }

    checksum =  0^sum;
    printf("checksum = 0x%x\n",checksum);

    fwrite(&checksum,4,1,fpr_dst);
    fclose(fpr);
    fclose(fpr_dst);
    
    return 0;
    
}

