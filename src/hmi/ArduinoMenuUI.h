#ifndef ARDUINOMENUUI_h
#define ARDUINOMENUUI_h

#include <hmi/MenuBuilder.h>
#include <hmi/ParameterEditor.h>

#include <menu.h>
#include <menuIO/adafruitGfxOut.h>

class Adafruit_ST7789;

class ArduinoMenuUI
{
public:
    struct EnterResult
    {
        enum class Type : uint8_t
        {
            None,
            Exit,
            Action
        };

        Type type = Type::None;
        MenuBuilder::ActionId actionId =
            MenuBuilder::NO_ACTION;
    };

    ArduinoMenuUI();

    bool begin(Adafruit_ST7789& display,
        ParameterEditor& editor,
        const MenuBuilder& menuDefinition);

    void show();
    void close();
    void move(int32_t direction);
    EnterResult enter();
    void poll();

    bool isInitialized() const;

private:
    static constexpr size_t MAX_DEPTH = 5;
    static constexpr size_t LABEL_LENGTH = 32;
    static constexpr size_t UNIT_LENGTH = 12;
    static constexpr uint8_t TEXT_SCALE = 2;
    static constexpr Menu::idx_t CHARACTER_WIDTH = 6 * TEXT_SCALE;
    static constexpr Menu::idx_t CHARACTER_HEIGHT = 9 * TEXT_SCALE;
    static constexpr size_t MAX_GROUPS =
        MenuBuilder::MAX_GROUPS;
    static constexpr size_t MAX_MENU_ITEMS =
        MAX_PARAMETERS +
        MenuBuilder::MAX_ACTIONS +
        (2 * (MAX_GROUPS - 1)) +
        1;
    static constexpr size_t MAX_SELECTION_OPTIONS =
        ParameterList::MAX_SELECTION_OPTIONS;

    bool initialized = false;
    Adafruit_ST7789* display = nullptr;
    uint8_t displayRotation = 0;

    Menu::prompt* parameterItems[MAX_PARAMETERS] = {};
    MenuBuilder::GroupId parameterGroups[MAX_PARAMETERS] = {};
    Menu::prompt* actionItems[MenuBuilder::MAX_ACTIONS] = {};
    MenuBuilder::ActionId actionIds[MenuBuilder::MAX_ACTIONS] = {};
    Menu::prompt* booleanOptions[MAX_PARAMETERS][2] = {};
    Menu::prompt* selectionOptionItems[MAX_SELECTION_OPTIONS] = {};
    char labels[MAX_PARAMETERS][LABEL_LENGTH] = {};
    char actionLabels[MenuBuilder::MAX_ACTIONS][LABEL_LENGTH] = {};
    char unitLabels[MAX_PARAMETERS][UNIT_LENGTH] = {};
    char groupLabels[MAX_GROUPS][LABEL_LENGTH] = {};
    char selectionOptionLabels
        [MAX_SELECTION_OPTIONS][LABEL_LENGTH] = {};

    Menu::prompt* menuItems[MAX_MENU_ITEMS] = {};
    Menu::menuNode* menuNodes[MAX_GROUPS] = {};
    Menu::Exit* backItems[MAX_GROUPS] = {};
    Menu::Exit* quitItem = nullptr;

    Menu::idx_t menuItemCounts[MAX_GROUPS] = {};
    size_t menuItemOffsets[MAX_GROUPS] = {};
    size_t menuItemCursors[MAX_GROUPS] = {};
    size_t selectionOptionsUsed = 0;

    Menu::menuNode* root = nullptr;
    Menu::navRoot* nav = nullptr;

    Menu::noInput input;

    Menu::panel panelDefinitions[1] = {
        {0, 0, 0, 0}
    };
    Menu::navNode* panelNodes[1] = {};
    Menu::panelsList panels;

    Menu::idx_t tops[MAX_DEPTH] = {};
    Menu::navNode path[MAX_DEPTH];

    Menu::adaGfxOut* screenOutput = nullptr;
    Menu::menuOut* outputPointers[1] = {};
    Menu::outputsList* outputs = nullptr;

    bool buildMenuTree(
        ParameterEditor& editor,
        const MenuBuilder& menuDefinition);

    bool appendMenuItem(
        MenuBuilder::GroupId group,
        Menu::prompt* item);

    Menu::prompt* createItem(
        ParameterDraft& draft,
        size_t index);

    Menu::prompt* createNumberItem(
        ParameterDraft& draft,
        const Parameter& parameter,
        const char* unit,
        const char* label);

    Menu::prompt* createSelectionItem(
        ParameterDraft& draft,
        const Parameter& parameter,
        const char* label);
};

#endif
