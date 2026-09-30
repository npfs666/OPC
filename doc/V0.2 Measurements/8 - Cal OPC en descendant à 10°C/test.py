adcTemperature = 10
calTemperatureADC = 20
systemPPMCoeff = -1
Rrtd = 100.061

ppm = (adcTemperature - calTemperatureADC) * systemPPMCoeff

Rrtd = Rrtd * (1 + ppm/1000000.0)

print(Rrtd)
