#pragma once
#include "HobbitProcessAnalyzer.h"

#include <vector>
#include <mutex>

#include <iomanip>
#include <functional>

#include <future>
#include <chrono>
class HobbitGameManager
{
private:
    HobbitProcessAnalyzer hobitProcessAnalyzer;
    using Listener = std::function<void()>;

    std::thread updateThread;
    std::atomic<bool> stopThread;
    // All derived classes

    // in game states
    uint32_t gameState;
    bool levelLoaded;
    bool levelFullyLoaded;
    uint32_t currentLevel;

    // events
    std::vector<Listener> listenersEnterNewLevel;
    std::vector<Listener> listenersExitLevel;
    std::vector<Listener> listenersOpenGame;
    std::vector<Listener> listenersCloseGame;

    // event functions
    void eventEnterNewLevel() {

        hobitProcessAnalyzer.startAnalyzingProcess();
        for (const auto& listener : listenersEnterNewLevel)
        {
            listener();
        }
    }
    void eventExitLevel() {

        hobitProcessAnalyzer.startAnalyzingProcess();
        for (const auto& listener : listenersExitLevel)
        {
            listener();
        }
    }

    void eventOpenGame()
    {
        hobitProcessAnalyzer.startAnalyzingProcess();
        for (const auto& listener : listenersOpenGame)
        {
            listener();
        }

    }
    void eventCloseGame()
    {
        hobitProcessAnalyzer.startAnalyzingProcess();
        for (const auto& listener : listenersCloseGame)
        {
            listener();
        }

    }

    uint32_t getGameState()
    {
        return gameState;
    }
    bool getLevelLoaded()
    {
        return levelLoaded;
    }
    bool getLevelFullyLoaded()
    {
        return levelFullyLoaded;
    }
    uint32_t getGameLevel()
    {
        return currentLevel;
    }

    void readInstanices()
    {
        gameState = hobitProcessAnalyzer.readData<uint32_t>(0x00762B58, sizeof(uint32_t)); // 0x00762B58: game state address
        currentLevel = hobitProcessAnalyzer.readData<uint32_t>(0x00762B5C, sizeof(uint32_t));  // 00762B5C: current level address
        levelLoaded = hobitProcessAnalyzer.readData<bool>(0x00760354, sizeof(uint32_t));  //0x0072C7D4: is loaded level address
        levelFullyLoaded = !hobitProcessAnalyzer.readData<bool>(0x00760354, sizeof(uint32_t));  //0x0072C7D4: is loaded level address
    }


    void start()
    {
        hobitProcessAnalyzer.startAnalyzingProcess();
    }
    void update()
    {
        bool wasHobbitOpen = false;
        bool isHobbitOpen = false;

        uint32_t previousState = -1;
        uint32_t currentState = -1;

        bool previousLevelLoaded = false;
        bool currentLevelLoaded = false;

        bool previousLevelFullyLoaded = false;
        bool currentLevelFullyLoaded = false;
        while (!stopThread)
        {
            // update speed
            std::this_thread::sleep_for(std::chrono::milliseconds(10));

            // check if game open
            isHobbitOpen = hobitProcessAnalyzer.isGameRunning();
            if (!isHobbitOpen)
            {
                if (isHobbitOpen != wasHobbitOpen)
                {
                    eventCloseGame();
                    std::cout << "Waiting for HobbitTM to be opened..." << std::endl;
                }
                wasHobbitOpen = isHobbitOpen;
                continue;
            }
            if (wasHobbitOpen)
            {
                eventOpenGame();
                wasHobbitOpen = isHobbitOpen;
            }

            // read instances of game (current level, etc.)
            readInstanices();

            // handle game states
            // game state
            currentState = getGameState();

            // level loaded
            currentLevelLoaded = getLevelLoaded();
            currentLevelFullyLoaded = getLevelFullyLoaded();

            // new level
            if (previousLevelFullyLoaded != currentLevelFullyLoaded && currentLevelFullyLoaded)
            {
                eventEnterNewLevel();
            }
            if (previousLevelLoaded != currentLevelLoaded && !currentLevelLoaded)
            {
                eventExitLevel();
            }

            previousState = currentState;
            previousLevelLoaded = currentLevelLoaded;
            previousLevelFullyLoaded = currentLevelFullyLoaded;
        }

    }
public:

    bool isGameRunning()
    {
        return hobitProcessAnalyzer.isGameRunning();
    }
    void addListenerEnterNewLevel(const Listener& listener) {
        listenersEnterNewLevel.push_back(listener);
    }
    void addListenerExitLevel(const Listener& listener) {
        listenersExitLevel.push_back(listener);
    }
    void addListenerOpenGame(const Listener& listener) {
        listenersOpenGame.push_back(listener);
    }
    void addListenerCloseGame(const Listener& listener) {
        listenersCloseGame.push_back(listener);
    }
    HobbitProcessAnalyzer* getHobbitProcessAnalyzer()
    {
        return &hobitProcessAnalyzer;
    }
    HobbitGameManager()
    {
        start();
        //updateThread = std::thread(update);
    }
    ~HobbitGameManager()
    {
        stopThread = true;
        updateThread.join();
    }
};