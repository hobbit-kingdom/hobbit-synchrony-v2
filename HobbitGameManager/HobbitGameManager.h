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

 
   
    std::atomic<bool> wasHobbitOpen = false;
    std::atomic<bool> isHobbitOpen = false;

    std::atomic<uint32_t> previousState = -1;
    std::atomic<uint32_t> currentState = -1;

    std::atomic<uint32_t> previousLevel = -1;
    std::atomic<uint32_t> currentLevel = -1;

    std::atomic<bool> wasLevelLoaded = false;
    std::atomic<bool> isLevelLoaded = false;
    void update()
    {
        while (!stopThread)
        {
            updateLoop();
        }
    }
    void updateLoop()
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
            wasHobbitOpen = !!isHobbitOpen;
            return;
        }
        if (wasHobbitOpen)
        {
            eventOpenGame();
            wasHobbitOpen = !!isHobbitOpen;
        }

        // read instances of game (current level, etc.)
        currentState = hobitProcessAnalyzer.readData<uint32_t>(0x00762B58, sizeof(uint32_t)); // 0x00762B58: game state address
        currentLevel = hobitProcessAnalyzer.readData<uint32_t>(0x00762B5C, sizeof(uint32_t));  // 0x00762B5C: current level address
        isLevelLoaded = hobitProcessAnalyzer.readData<bool>(0x00760354, sizeof(uint32_t));  //0x00760354: is loaded level address
        //isLevelLoaded = !hobitProcessAnalyzer.readData<bool>(0x0076035C, sizeof(uint32_t));  //0x0076035C: is loaded level address

    
        if (wasLevelLoaded != isLevelLoaded && isLevelLoaded)
        {
            if (isLevelLoaded)
                eventEnterNewLevel();
            else
                eventExitLevel();
        }


        previousState = !!currentState;
        previousLevel = !!currentLevel;
        wasLevelLoaded = !!isLevelLoaded;
    }
public:
    void start()
    {
        stopThread = false; // Initialize stopThread to false
        hobitProcessAnalyzer.startAnalyzingProcess();
        updateThread = std::thread(&HobbitGameManager::update, this); // Start the update thread
    }
    bool isOnLevel()
    {
        return isLevelLoaded;
    }
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
        while (!isGameRunning())
        {
            std::cout << "You must open the game!" << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(2)); // Sleep for 2 seconds
        }
    }
    ~HobbitGameManager()
    {
        stopThread = true;
        updateThread.join();
    }
};