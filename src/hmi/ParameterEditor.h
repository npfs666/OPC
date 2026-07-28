#ifndef PARAMETER_EDITOR_H
#define PARAMETER_EDITOR_H

#include <hmi/ParameterList.h>
#include <Hardware/pinout.h>

struct ParameterDraft
{
    const Parameter* parameter = nullptr;

    bool booleanValue = false;
    int32_t integerValue = 0;
    double_t numberValue = 0.0;
    int32_t selectionValue = 0;
};

class ParameterEditor
{
public:
    void begin(const ParameterList& parameters);
    void capture();
    bool validate() const;
    bool apply();

    size_t count() const;
    ParameterDraft& get(size_t index);
    const ParameterDraft& get(size_t index) const;

    const ParameterDraft* find(
        const char* ownerKey,
        const char* parameterKey) const;

private:
    ParameterDraft drafts[MAX_PARAMETERS];
    size_t draftCount = 0;
};

#endif
