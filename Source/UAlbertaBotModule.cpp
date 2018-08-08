/* 
 *----------------------------------------------------------------------
 * Steamhammer
 *----------------------------------------------------------------------
 */

#include "Common.h"
#include "UAlbertaBotModule.h"
#include "JSONTools.h"
#include "ParseUtils.h"
#include "UnitUtil.h"

using namespace UAlbertaBot;

// This gets called when the bot starts!
void UAlbertaBotModule::onStart()
{
    // Initialize SparCraft, the combat simulation package
	// SparCraft::init();

    // Initialize BOSS, the Build Order Search System
    BOSS::init();

	// Call BWTA to read and analyze the current map
	BWTA::readMap();
	BWTA::analyze();
	BWTA::buildChokeNodes();

    // Parse the bot's configuration file.
	// Change this file path to point to your config file.
    // Any relative path name will be relative to Starcraft installation folder
	ParseUtils::ParseConfigFile(Config::ConfigFile::ConfigFileLocation);

    // Set our BWAPI options here    
	BWAPI::Broodwar->setLocalSpeed(Config::BWAPIOptions::SetLocalSpeed);
	BWAPI::Broodwar->setFrameSkip(Config::BWAPIOptions::SetFrameSkip);
    
    if (Config::BWAPIOptions::EnableCompleteMapInformation)
    {
        BWAPI::Broodwar->enableFlag(BWAPI::Flag::CompleteMapInformation);
    }

    if (Config::BWAPIOptions::EnableUserInput)
    {
        BWAPI::Broodwar->enableFlag(BWAPI::Flag::UserInput);
    }

    if (Config::BotInfo::PrintInfoOnStart)
    {
        BWAPI::Broodwar->printf("%s by %s, based on Steamhammer 1.2.2/UAlbertaBot.", Config::BotInfo::BotName.c_str(), Config::BotInfo::Authors.c_str());
    }

    if (Config::Modules::UsingStrategyIO)
    {
        StrategyManager::Instance().readResults();
        StrategyManager::Instance().setLearnedStrategy();
    }

	StrategyManager::Instance().setOpeningGroup();
	//BWAPI::Broodwar->setLatCom(false); //Turns off latency compensation -- before we do this we need to write our own latency compensation code
}

void UAlbertaBotModule::onEnd(bool isWinner) 
{
	StrategyManager::Instance().onEnd(isWinner);
}

void UAlbertaBotModule::onFrame()
{
    char red = '\x08';
    char green = '\x07';
    char white = '\x04';

    if (!Config::ConfigFile::ConfigFileFound)
    {
        BWAPI::Broodwar->drawBoxScreen(0,0,450,100, BWAPI::Colors::Black, true);
        BWAPI::Broodwar->setTextSize(BWAPI::Text::Size::Huge);
        BWAPI::Broodwar->drawTextScreen(10, 5, "%c%s Config File Not Found", red, Config::BotInfo::BotName.c_str());
        BWAPI::Broodwar->setTextSize(BWAPI::Text::Size::Default);
        BWAPI::Broodwar->drawTextScreen(10, 30, "%c%s will not run without its configuration file", white, Config::BotInfo::BotName.c_str());
        BWAPI::Broodwar->drawTextScreen(10, 45, "%cCheck that the file below exists. Incomplete paths are relative to Starcraft directory", white);
        BWAPI::Broodwar->drawTextScreen(10, 60, "%cYou can change this file location in Config::ConfigFile::ConfigFileLocation", white);
        BWAPI::Broodwar->drawTextScreen(10, 75, "%cFile Not Found (or is empty): %c %s", white, green, Config::ConfigFile::ConfigFileLocation.c_str());
        return;
    }
    else if (!Config::ConfigFile::ConfigFileParsed)
    {
        BWAPI::Broodwar->drawBoxScreen(0,0,450,100, BWAPI::Colors::Black, true);
        BWAPI::Broodwar->setTextSize(BWAPI::Text::Size::Huge);
        BWAPI::Broodwar->drawTextScreen(10, 5, "%c%s Config File Parse Error", red, Config::BotInfo::BotName.c_str());
        BWAPI::Broodwar->setTextSize(BWAPI::Text::Size::Default);
        BWAPI::Broodwar->drawTextScreen(10, 30, "%c%s will not run without a properly formatted configuration file", white, Config::BotInfo::BotName.c_str());
        BWAPI::Broodwar->drawTextScreen(10, 45, "%cThe configuration file was found, but could not be parsed. Check that it is valid JSON", white);
        BWAPI::Broodwar->drawTextScreen(10, 60, "%cFile Not Parsed: %c %s", white, green, Config::ConfigFile::ConfigFileLocation.c_str());
        return;
    }

	GameCommander::Instance().update(); 
}

void UAlbertaBotModule::onUnitDestroy(BWAPI::Unit unit)
{
	GameCommander::Instance().onUnitDestroy(unit);
}

void UAlbertaBotModule::onUnitMorph(BWAPI::Unit unit)
{
	GameCommander::Instance().onUnitMorph(unit);
}

void UAlbertaBotModule::onSendText(std::string text) 
{ 
	ParseUtils::ParseTextCommand(text);
}

void UAlbertaBotModule::onUnitCreate(BWAPI::Unit unit)
{ 
	GameCommander::Instance().onUnitCreate(unit);
}

void UAlbertaBotModule::onUnitComplete(BWAPI::Unit unit)
{
	GameCommander::Instance().onUnitComplete(unit);
}

void UAlbertaBotModule::onUnitShow(BWAPI::Unit unit)
{ 
	GameCommander::Instance().onUnitShow(unit);
}

void UAlbertaBotModule::onUnitHide(BWAPI::Unit unit)
{ 
	GameCommander::Instance().onUnitHide(unit);
}

void UAlbertaBotModule::onUnitRenegade(BWAPI::Unit unit)
{ 
	GameCommander::Instance().onUnitRenegade(unit);
}