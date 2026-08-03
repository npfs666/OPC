#include <hmi/ArduinoMenuUI.h>

#include <Adafruit_ST7789.h>
#include <hmi/DisplayTextCodec.h>

namespace
{
    constexpr uint16_t COLOR_GREY = 0x8410;

    const Menu::colorDef<uint16_t> MENU_COLORS[Menu::nColors] = {
        {
            {ST77XX_BLACK, ST77XX_BLACK},
            {ST77XX_BLACK, COLOR_GREY, ST77XX_RED}
        },
        {
            {ST77XX_CYAN, ST77XX_CYAN},
            {ST77XX_WHITE, ST77XX_WHITE, ST77XX_WHITE}
        },
        {
            {ST77XX_WHITE, ST77XX_WHITE},
            {ST77XX_YELLOW, ST77XX_YELLOW, ST77XX_RED}
        },
        {
            {ST77XX_WHITE, ST77XX_WHITE},
            {ST77XX_WHITE, ST77XX_YELLOW, ST77XX_YELLOW}
        },
        {
            {ST77XX_WHITE, ST77XX_CYAN},
            {ST77XX_BLACK, COLOR_GREY, ST77XX_WHITE}
        },
        {
            {ST77XX_WHITE, ST77XX_YELLOW},
            {ST77XX_CYAN, ST77XX_BLACK, ST77XX_BLACK}
        }
    };
}

ArduinoMenuUI::ArduinoMenuUI()
    : panels(
        panelDefinitions,
        panelNodes,
        sizeof(panelDefinitions) / sizeof(panelDefinitions[0]))
{
}

bool ArduinoMenuUI::begin(Adafruit_ST7789 &display,
                          ParameterEditor &editor,
                          const MenuBuilder &menuDefinition)
{
    if (initialized)
        return true;

    if (!buildMenuTree(
            editor,
            menuDefinition))
    {
        return false;
    }

    panelDefinitions[0].w = static_cast<Menu::idx_t>(
        display.width() / CHARACTER_WIDTH);
    panelDefinitions[0].h = static_cast<Menu::idx_t>(
        display.height() / CHARACTER_HEIGHT);

    display.cp437(true);
    display.setTextSize(TEXT_SCALE);
    display.setTextWrap(false);

    this->display = &display;
    displayRotation = display.getRotation();

    screenOutput = new Menu::adaGfxOut(
        display,
        MENU_COLORS,
        tops,
        panels,
        CHARACTER_WIDTH,
        CHARACTER_HEIGHT);

    outputPointers[0] = screenOutput;
    outputs = new Menu::outputsList(
        outputPointers,
        sizeof(outputPointers) / sizeof(outputPointers[0]));

    nav = new Menu::navRoot(
        *root,
        path,
        MAX_DEPTH,
        input,
        *outputs);

    nav->canExit = true;

    initialized = true;

    return true;
}

void ArduinoMenuUI::show()
{
    if (!initialized ||
        nav == nullptr ||
        display == nullptr)
    {
        return;
    }

    display->setRotation(displayRotation);
    display->setFont(nullptr);
    display->cp437(true);
    display->setTextSize(TEXT_SCALE);
    display->setTextWrap(false);

    if (nav->sleepTask != nullptr)
        nav->idleOff();

    nav->reset();

    display->fillScreen(ST77XX_BLACK);

    nav->refresh();
    nav->poll();
}

void ArduinoMenuUI::close()
{
    if (!initialized || nav == nullptr)
        return;

    if (nav->sleepTask != nullptr)
        return;

    if (nav->navFocus != &nav->active())
        nav->doNav(Menu::navCmd(Menu::escCmd));

    while (nav->level > 0)
        nav->doNav(Menu::navCmd(Menu::escCmd));

    if (nav->sleepTask == nullptr)
        nav->idleOn(nav->idleTask);
}

void ArduinoMenuUI::move(int32_t direction)
{
    if (!initialized || nav == nullptr)
        return;

    if (direction > 0)
        nav->doNav(Menu::navCmd(Menu::downCmd));
    else if (direction < 0)
        nav->doNav(Menu::navCmd(Menu::upCmd));
}

bool ArduinoMenuUI::enter()
{
    if (!initialized || nav == nullptr)
        return false;

    const bool exitRequested =
        nav->level == 0 &&
        quitItem != nullptr &&
        &nav->selected() == quitItem;

    nav->doNav(Menu::navCmd(Menu::enterCmd));

    return exitRequested;
}

void ArduinoMenuUI::poll()
{
    if (!initialized || nav == nullptr)
        return;

    nav->poll();
}

bool ArduinoMenuUI::isInitialized() const
{
    return initialized;
}

bool ArduinoMenuUI::buildMenuTree(
    ParameterEditor& editor,
    const MenuBuilder& menuDefinition)
{
    if (!menuDefinition.isInitialized())
        return false;

    const size_t groupCount =
        menuDefinition.groupCount();

    if (groupCount == 0 ||
        groupCount > MAX_GROUPS)
    {
        return false;
    }

    selectionOptionsUsed = 0;
    quitItem = nullptr;

    for (size_t i = 0;
         i < MAX_SELECTION_OPTIONS;
         i++)
    {
        selectionOptionItems[i] = nullptr;
    }

    for (size_t i = 0; i < MAX_GROUPS; i++)
    {
        menuNodes[i] = nullptr;
        backItems[i] = nullptr;
        menuItemCounts[i] = 0;
        menuItemOffsets[i] = 0;
        menuItemCursors[i] = 0;
    }

    for (size_t i = 0; i < MAX_PARAMETERS; i++)
    {
        parameterItems[i] = nullptr;
        parameterGroups[i] =
            MenuBuilder::INVALID_GROUP;
    }

    for (size_t i = 1; i < groupCount; i++)
    {
        const MenuBuilder::Group* group =
            menuDefinition.getGroup(
                static_cast<MenuBuilder::GroupId>(i));

        if (group == nullptr)
            return false;

        if (menuDefinition.getGroup(
                group->parent) == nullptr)
        {
            return false;
        }

        menuItemCounts[group->parent]++;
    }

    const size_t parameterCount =
        editor.count() < MAX_PARAMETERS
            ? editor.count()
            : MAX_PARAMETERS;

    for (size_t i = 0; i < parameterCount; i++)
    {
        ParameterDraft& draft = editor.get(i);

        if (draft.parameter == nullptr)
            return false;

        const MenuBuilder::GroupId group =
            menuDefinition.findGroupForOwner(
                draft.parameter->ownerKey);

        if (group == MenuBuilder::INVALID_GROUP)
            return false;

        if (menuDefinition.getGroup(group) == nullptr)
            return false;

        Menu::prompt* item =
            createItem(draft, i);

        if (item == nullptr)
            return false;

        if (draft.parameter->readOnly)
            item->disable();

        parameterItems[i] = item;
        parameterGroups[i] = group;
        menuItemCounts[group]++;
    }

    const MenuBuilder::GroupId rootGroup =
        menuDefinition.root();

    if (rootGroup ==
            MenuBuilder::INVALID_GROUP ||
        menuDefinition.getGroup(
            rootGroup) == nullptr)
    {
        return false;
    }

    menuItemCounts[rootGroup]++;

    for (size_t i = 1; i < groupCount; i++)
        menuItemCounts[i]++;

    size_t totalMenuItems = 0;

    for (size_t i = 0; i < groupCount; i++)
    {
        menuItemOffsets[i] = totalMenuItems;
        menuItemCursors[i] = totalMenuItems;

        totalMenuItems +=
            static_cast<size_t>(
                menuItemCounts[i]);

        if (totalMenuItems > MAX_MENU_ITEMS)
            return false;
    }

    for (size_t i = 0; i < groupCount; i++)
    {
        const MenuBuilder::Group* group =
            menuDefinition.getGroup(
                static_cast<MenuBuilder::GroupId>(i));

        if (group == nullptr ||
            group->name == nullptr)
        {
            return false;
        }

        DisplayTextCodec::utf8ToCp437(
            group->name,
            groupLabels[i],
            LABEL_LENGTH);

        if (groupLabels[i][0] == '\0')
            return false;

        menuNodes[i] = new Menu::menuNode(
            groupLabels[i],
            menuItemCounts[i],
            &menuItems[menuItemOffsets[i]]);
    }

    for (size_t i = 1; i < groupCount; i++)
    {
        const MenuBuilder::Group* group =
            menuDefinition.getGroup(
                static_cast<MenuBuilder::GroupId>(i));

        if (!appendMenuItem(
                group->parent,
                menuNodes[i]))
        {
            return false;
        }
    }

    for (size_t i = 0; i < parameterCount; i++)
    {
        if (parameterItems[i] == nullptr)
            continue;

        if (!appendMenuItem(
                parameterGroups[i],
                parameterItems[i]))
        {
            return false;
        }
    }

    quitItem = new Menu::Exit("Quitter");

    if (!appendMenuItem(
            rootGroup,
            quitItem))
    {
        return false;
    }

    for (size_t i = 1; i < groupCount; i++)
    {
        backItems[i] =
            new Menu::Exit("< Retour");

        if (!appendMenuItem(
                static_cast<MenuBuilder::GroupId>(i),
                backItems[i]))
        {
            return false;
        }
    }

    for (size_t i = 0; i < groupCount; i++)
    {
        const size_t expectedEnd =
            menuItemOffsets[i] +
            static_cast<size_t>(
                menuItemCounts[i]);

        if (menuItemCursors[i] != expectedEnd)
            return false;
    }

    root = menuNodes[rootGroup];

    return root != nullptr &&
           menuItemCounts[rootGroup] > 0;
}

bool ArduinoMenuUI::appendMenuItem(
    MenuBuilder::GroupId group,
    Menu::prompt* item)
{
    if (group == MenuBuilder::INVALID_GROUP ||
        group >= MAX_GROUPS ||
        item == nullptr)
    {
        return false;
    }

    const size_t end =
        menuItemOffsets[group] +
        static_cast<size_t>(
            menuItemCounts[group]);

    if (menuItemCursors[group] >= end)
        return false;

    menuItems[menuItemCursors[group]] = item;
    menuItemCursors[group]++;

    return true;
}

Menu::prompt* ArduinoMenuUI::createItem(
    ParameterDraft& draft,
    size_t index)
{
    const Parameter& parameter = *draft.parameter;

    const char* name =
        parameter.name != nullptr
            ? parameter.name
            : parameter.key;

    if (name == nullptr)
        return nullptr;

    DisplayTextCodec::utf8ToCp437(
        name,
        labels[index],
        LABEL_LENGTH);

    if (labels[index][0] == '\0')
        return nullptr;

    unitLabels[index][0] = '\0';

    switch (parameter.type)
    {
    case Parameter::Type::Bool:
        booleanOptions[index][0] =
            new Menu::menuValue<bool>("Non", false);
        booleanOptions[index][1] =
            new Menu::menuValue<bool>("Oui", true);

        return new Menu::toggle<bool>(
            labels[index],
            draft.booleanValue,
            2,
            booleanOptions[index]);

    case Parameter::Type::Integer:
    {
        const char* unit =
            parameter.data.integer.unit != nullptr
                ? parameter.data.integer.unit
                : "";

        DisplayTextCodec::utf8ToCp437(
            unit,
            unitLabels[index],
            UNIT_LENGTH);

        return new Menu::menuField<int32_t>(
            draft.integerValue,
            labels[index],
            unitLabels[index],
            parameter.data.integer.minimum,
            parameter.data.integer.maximum,
            parameter.data.integer.step,
            0);
    }

    case Parameter::Type::Double:
    {
        const char* unit =
            parameter.data.number.unit != nullptr
                ? parameter.data.number.unit
                : "";

        DisplayTextCodec::utf8ToCp437(
            unit,
            unitLabels[index],
            UNIT_LENGTH);

        return createNumberItem(
            draft,
            parameter,
            unitLabels[index],
            labels[index]);
    }

    case Parameter::Type::Selection:
        return createSelectionItem(
            draft,
            parameter,
            labels[index]);
    }

    return nullptr;
}

Menu::prompt* ArduinoMenuUI::createSelectionItem(
    ParameterDraft& draft,
    const Parameter& parameter,
    const char* label)
{
    const Parameter::Data::Selection& selection =
        parameter.data.selection;

    if (selection.options == nullptr ||
        selection.count == 0)
    {
        return nullptr;
    }

    bool selectedValueFound = false;

    for (uint8_t i = 0;
         i < selection.count;
         i++)
    {
        if (selection.options[i].value ==
            draft.selectionValue)
        {
            selectedValueFound = true;
            break;
        }
    }

    if (!selectedValueFound)
        return nullptr;

    if (selectionOptionsUsed +
            selection.count >
        MAX_SELECTION_OPTIONS)
    {
        return nullptr;
    }

    const size_t firstOption =
        selectionOptionsUsed;

    for (uint8_t i = 0;
         i < selection.count;
         i++)
    {
        DisplayTextCodec::utf8ToCp437(
            selection.options[i].name,
            selectionOptionLabels[
                selectionOptionsUsed],
            LABEL_LENGTH);

        if (selectionOptionLabels[
                selectionOptionsUsed][0] == '\0')
        {
            return nullptr;
        }

        selectionOptionItems[
            selectionOptionsUsed] =
            new Menu::menuValue<int32_t>(
                selectionOptionLabels[
                    selectionOptionsUsed],
                selection.options[i].value);

        selectionOptionsUsed++;
    }

    return new Menu::select<int32_t>(
        label,
        draft.selectionValue,
        selection.count,
        &selectionOptionItems[firstOption]);
}

Menu::prompt* ArduinoMenuUI::createNumberItem(
    ParameterDraft& draft,
    const Parameter& parameter,
    const char* unit,
    const char* label)
{
    const double_t minimum = parameter.data.number.minimum;
    const double_t maximum = parameter.data.number.maximum;
    const double_t fineStep =
        parameter.data.number.step;

    const bool hasFineAdjustment =
        parameter.data.number.decimals > 0 &&
        fineStep < 1.0;

    const double_t coarseStep =
        hasFineAdjustment
            ? 1.0
            : fineStep;

    const double_t tuneStep =
        hasFineAdjustment
            ? fineStep
            : 0.0;

    switch (parameter.data.number.decimals)
    {
    case 0:
        return new Menu::decPlaces<0>::menuField<double_t>(
            draft.numberValue,
            label,
            unit,
            minimum,
            maximum,
            coarseStep,
            tuneStep);

    case 1:
        return new Menu::decPlaces<1>::menuField<double_t>(
            draft.numberValue,
            label,
            unit,
            minimum,
            maximum,
            coarseStep,
            tuneStep);

    case 2:
        return new Menu::decPlaces<2>::menuField<double_t>(
            draft.numberValue,
            label,
            unit,
            minimum,
            maximum,
            coarseStep,
            tuneStep);

    default:
        return new Menu::decPlaces<3>::menuField<double_t>(
            draft.numberValue,
            label,
            unit,
            minimum,
            maximum,
            coarseStep,
            tuneStep);
    }
}
