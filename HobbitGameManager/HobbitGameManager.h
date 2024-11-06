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
public:
    using Listener = std::function<void()>;

    HobbitGameManager()
    {
      
    }
    ~HobbitGameManager()
    {
        stop();
    }

    void start()
    {
        // Wait Until the game is open
        while (!isGameRunning())
        {
            std::cout << "You must open the game!" << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(2)); // Sleep for 2 seconds
        }
        
        // Set proces and Stack Address
        hobitProcessAnalyzer.updatePtrToProcess();
        hobitProcessAnalyzer.updateObjectStackAddress();

        // Start Update Thread
        stopThread = false; // Initialize stopThread to false
        updateThread = std::thread(&HobbitGameManager::update, this); // Start the update thread
    }
    void stop()
    {
        stopThread = true;
        if (updateThread.joinable())
            updateThread.join();
    }
    bool isOnLevel()
    {
        return   (!!hobitProcessAnalyzer.readData<bool>(0x007A59C8, sizeof(uint32_t)) && !isLevelLoading);
    }
    bool isGameRunning()
    {
        return hobitProcessAnalyzer.isGameRunning();
    }

    // Add the listeners Functions
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


    HobbitProcessAnalyzer& getHobbitProcessAnalyzer()
    {
        return hobitProcessAnalyzer;
    }

private:
    HobbitProcessAnalyzer hobitProcessAnalyzer; // TO DO make this thread safe

    std::thread updateThread;
    std::atomic<bool> stopThread;

    // events
    std::vector<Listener> listenersEnterNewLevel;
    std::vector<Listener> listenersExitLevel;
    std::vector<Listener> listenersOpenGame;
    std::vector<Listener> listenersCloseGame;

    // event functions
    void eventEnterNewLevel() {
        isLevelLoaded = true;
        hobitProcessAnalyzer.updateObjectStackAddress();
        for (const auto& listener : listenersEnterNewLevel)
        {
            listener();
        }
    }
    void eventExitLevel() {
        isLevelLoaded = false;
        hobitProcessAnalyzer.updateObjectStackAddress();
        for (const auto& listener : listenersExitLevel)
        {
            listener();
        }
    }

    void eventOpenGame()
    {
        isLevelLoaded = false;
        hobitProcessAnalyzer.updatePtrToProcess();
        hobitProcessAnalyzer.updateObjectStackAddress();
        for (const auto& listener : listenersOpenGame)
        {
            listener();
        }

    }
    void eventCloseGame()
    {
        isLevelLoaded = false;
        hobitProcessAnalyzer.updatePtrToProcess();
        hobitProcessAnalyzer.updateObjectStackAddress();
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

    std::atomic<bool> wasLevelEnded = false;
    std::atomic<bool> isLevelEnded = false;

    std::atomic<bool> wasLevelLoading = false;
    std::atomic<bool> isLevelLoading = false;

    std::atomic<bool> isLevelLoaded = false;
    std::atomic<bool> wasLevelLoaded = false;


    void updateGameState()
    {
        isHobbitOpen = hobitProcessAnalyzer.isGameRunning();
        if (!isHobbitOpen)
        {
            if (isHobbitOpen != wasHobbitOpen)
            {
                eventCloseGame();
            }
            wasHobbitOpen = !!isHobbitOpen;
            return;
        }
        if (wasHobbitOpen)
        {
            eventOpenGame();
            wasHobbitOpen = !!isHobbitOpen;
        }
    }
    void updateLevelState()
    {
        if (!isGameRunning())
            return;

        // read instances of game (current level, etc.)
        currentState = hobitProcessAnalyzer.readData<uint32_t>(0x00762B58, sizeof(uint32_t)); // 0x00762B58: game state address
        currentLevel = hobitProcessAnalyzer.readData<uint32_t>(0x00762B5C, sizeof(uint32_t));  // 0x00762B5C: current level address
        isLevelEnded = !hobitProcessAnalyzer.readData<bool>(0x00760354, sizeof(uint32_t));  //0x00760354: is loaded level address
        isLevelLoading = hobitProcessAnalyzer.readData<bool>(0x0076035C, sizeof(uint32_t));  //0x0x0072F048: is loaded level address

        bool isLevelAssigned = !!hobitProcessAnalyzer.readData<bool>(0x007A59C8, sizeof(uint32_t));  //0x0x0072F048: is loaded level address
        isLevelLoaded = (isLevelAssigned && !isLevelLoading);

        if (wasLevelEnded != isLevelEnded && isLevelEnded)
            eventExitLevel();
        if (wasLevelLoaded != isLevelLoaded && isLevelLoaded)
            eventEnterNewLevel();

        previousState = !!currentState;
        previousLevel = !!currentLevel;
        wasLevelEnded = !!isLevelEnded;
        wasLevelLoading = !!isLevelLoading;
        wasLevelLoaded = !!isLevelLoaded;
    }

    void update()
    {
        while (!stopThread)
        {
            updateGameState();
            updateLevelState();

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
};