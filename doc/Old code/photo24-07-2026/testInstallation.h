#include "Installation.h"

class testInstallation : public Installation
{
public:

    void build(ProcessControl& controller) override;

    void buildMenu(MenuBuilder& menu) override;

    void load(Storage& storage) override;

    void save(Storage& storage) override;

    void factoryReset() override;

private:

    virtual const char* name() const = 0;
};