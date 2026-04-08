#include "pciecommunication.h"
#include <QByteArray>
#include <QDebug>


PcieCommunication::PcieCommunication(QObject *parent)
    : QObject(parent)
{

}

PcieCommunication::~PcieCommunication()
{

}

int PcieCommunication::pcieOpen(QString basePath)
{
    int status = 1;

    wchar_t  user_path_w[MAX_PATH+1];
    wchar_t  c2h0_path_w[MAX_PATH+1];
    wchar_t  h2c0_path_w[MAX_PATH+1];
    QString user_path = basePath + "\\user";
    QString c2h0_path = basePath + "\\c2h_0";
    QString h2c0_path = basePath + "\\h2c_0";

    mbstowcs(user_path_w, user_path.toUtf8().data(),  user_path.size() + 1);
    mbstowcs(h2c0_path_w, h2c0_path.toUtf8().data(),  h2c0_path.size() + 1);
    mbstowcs(c2h0_path_w, c2h0_path.toUtf8().data(),  c2h0_path.size() + 1);

    xdma.user = INVALID_HANDLE_VALUE;
    xdma.c2h0 = INVALID_HANDLE_VALUE;
    xdma.h2c0 = INVALID_HANDLE_VALUE;

    xdma.user = CreateFile(user_path_w,GENERIC_READ|GENERIC_WRITE,0,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if(xdma.user == INVALID_HANDLE_VALUE)
        return -1;
    xdma.c2h0 = CreateFile(c2h0_path_w,GENERIC_READ ,0,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if(xdma.c2h0 == INVALID_HANDLE_VALUE)
        return -2;
    xdma.h2c0 = CreateFile(h2c0_path_w,GENERIC_WRITE,0,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if(xdma.h2c0 == INVALID_HANDLE_VALUE)
        return -3;

    return status;
}

void PcieCommunication::pcieClose()
{
    if (xdma.user != INVALID_HANDLE_VALUE) {
        CloseHandle(xdma.user);
        xdma.user = INVALID_HANDLE_VALUE;
    }
    if (xdma.c2h0 != INVALID_HANDLE_VALUE) {
        CloseHandle(xdma.c2h0);
        xdma.c2h0 = INVALID_HANDLE_VALUE;
    }
    if (xdma.h2c0 != INVALID_HANDLE_VALUE) {
        CloseHandle(xdma.h2c0);
        xdma.h2c0 = INVALID_HANDLE_VALUE;
    }
}

int PcieCommunication::writeDevice(HANDLE device, unsigned int address, DWORD size, BYTE *buffer)
{
    DWORD wr_size = 0;
    unsigned int transfers;
    unsigned int i;
    transfers = (unsigned int)(size / MAX_BYTES_PER_TRANSFER);

    if (INVALID_SET_FILE_POINTER == SetFilePointer(device, address, NULL, FILE_BEGIN)) {
        fprintf(stderr, "Error setting file pointer, win32 error code: %ld\n", GetLastError());
        return -3;
    }


    for (i = 0; i<transfers; i++)
    {
        if (!WriteFile(device, (void *)(buffer + i*MAX_BYTES_PER_TRANSFER), MAX_BYTES_PER_TRANSFER, &wr_size, NULL))
        {
            return -1;
        }
        if (wr_size != MAX_BYTES_PER_TRANSFER)
        {
            return -2;
        }
    }
    if (!WriteFile(device, (void *)(buffer + i*MAX_BYTES_PER_TRANSFER), (DWORD)(size - i*MAX_BYTES_PER_TRANSFER), &wr_size, NULL))
    {
        return -1;
    }
    if (wr_size != (size - i*MAX_BYTES_PER_TRANSFER))
    {
        return -2;
    }
    return size;
}

int PcieCommunication::readDevice(HANDLE device, unsigned int address, DWORD size, BYTE *buffer)
{
    DWORD rd_size = 0;
    unsigned int transfers;
    unsigned int i=0;

    LARGE_INTEGER addr;
    memset(&addr,0,sizeof(addr));
    addr.QuadPart = address;
    if ((int)INVALID_SET_FILE_POINTER == SetFilePointerEx(device, addr, NULL, FILE_BEGIN)) {
        return -3;
    }
    transfers = (unsigned int)(size / MAX_BYTES_PER_TRANSFER);
    for (i = 0; i<transfers; i++)
    {
        if (!ReadFile(device, (void *)(buffer + i*MAX_BYTES_PER_TRANSFER), (DWORD)MAX_BYTES_PER_TRANSFER, &rd_size, NULL))
        {
            return -1;
        }
        if (rd_size != MAX_BYTES_PER_TRANSFER)
        {
            return -2;
        }
    }
    if (!ReadFile(device, (void *)(buffer + i*MAX_BYTES_PER_TRANSFER), (DWORD)(size - i*MAX_BYTES_PER_TRANSFER), &rd_size,NULL))
    {
        return -4;
    }
    if (rd_size != (size - i*MAX_BYTES_PER_TRANSFER))
    {
        return -5;
    }
    return size;
}



int PcieCommunication::h2cTransfer(unsigned int address, unsigned int size, unsigned char *buffer)
{
    return writeDevice(xdma.h2c0, address, size, buffer);
}

int PcieCommunication::c2hTransfer(unsigned int address, unsigned int size, unsigned char *buffer)
{
    return readDevice(xdma.c2h0, address, size, buffer);
}


int PcieCommunication::writeUser(unsigned int address, unsigned int size , unsigned char *buffer)
{
    return writeDevice(xdma.user, address , size , buffer);
}


int PcieCommunication::readUser(unsigned int address, unsigned int size, unsigned char *buffer)
{
    return readDevice(xdma.user, address , size , buffer);
}


