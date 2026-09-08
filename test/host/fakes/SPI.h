#ifndef TEST_FAKE_SPI_H
#define TEST_FAKE_SPI_H

// Objets requis par le brochage v0.2 ; aucun transfert SPI dans les tests hôte.
class SPIClassRP2040
{
};

inline SPIClassRP2040 SPI;
inline SPIClassRP2040 SPI1;

#endif
