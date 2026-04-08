#include "pciefunctionpackage.h"

PcieFunctionPackage::PcieFunctionPackage()
{
}

int PcieFunctionPackage::read_device(HANDLE device, unsigned int address, DWORD size, BYTE *buffer)
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
        return -1;
    }
    if (rd_size != (size - i*MAX_BYTES_PER_TRANSFER))
    {
        return -2;
    }
    return size;
}

int PcieFunctionPackage::write_device(HANDLE device, unsigned int address, DWORD size, BYTE *buffer)
{
    DWORD wr_size = 0;
    unsigned int transfers;
    unsigned int i;
    transfers = (unsigned int)(size / MAX_BYTES_PER_TRANSFER);
    if (INVALID_SET_FILE_POINTER == SetFilePointer(device, address, NULL, FILE_BEGIN)) {
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
