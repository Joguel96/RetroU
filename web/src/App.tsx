import React, { useEffect } from 'react';
import { RetroProvider, useRetro } from './context/RetroContext';
import { GlassBackground } from './components/GlassBackground';
import { BottomNavBar } from './components/BottomNavBar';
import { GameModal } from './components/GameModal';
import { EmulatorModal } from './components/EmulatorModal';
import { HomeView } from './views/HomeView';
import { ConsolesView } from './views/ConsolesView';
import { SettingsView } from './views/SettingsView';
import { soundManager } from './utils/audio';
import { useGamepadNavigation } from './gamepad/useGamepadNavigation';

const MainShell: React.FC = () => {
  const { 
    currentView, 
    setCurrentView,
    selectedGame, 
    setSelectedGame, 
    runningGame, 
    setRunningGame,
    games,
    hoveredGame,
    focusedGame,
    focusedIndex,
    setFocusedIndex,
    toggleFavorite,
    settingsTab,
    setSettingsTab
  } = useRetro();

  const { connected } = useGamepadNavigation();
  const keyboardEnabled = new URLSearchParams(window.location.search).get('keyboard') === '1';

  // Keyboard and pointer are development fallbacks only. The real input is
  // the Wii U GamePad / Pro Controller through the Gamepad API.
  useEffect(() => {
    const root = document.documentElement;
    root.classList.toggle('gamepad-mode', !keyboardEnabled);

    if (keyboardEnabled) return;

    const blockTrustedKeyboard = (event: KeyboardEvent) => {
      if (event.isTrusted) {
        event.preventDefault();
        event.stopImmediatePropagation();
      }
    };

    window.addEventListener('keydown', blockTrustedKeyboard, true);
    window.addEventListener('keyup', blockTrustedKeyboard, true);
    return () => {
      window.removeEventListener('keydown', blockTrustedKeyboard, true);
      window.removeEventListener('keyup', blockTrustedKeyboard, true);
    };
  }, [keyboardEnabled]);

  // Action bus for synthetic Gamepad events. Views keep their spatial logic;
  // trusted physical keyboard events are blocked above.
  useEffect(() => {
    const handleGlobalKeyDown = (e: KeyboardEvent) => {
      if (e.isTrusted) return;

      // If running emulator, Esc exits emulator
      if (runningGame) {
        if (e.key === 'Escape' || e.key === 'b' || e.key === 'B') {
          e.preventDefault();
          setRunningGame(null);
        }
        return;
      }

      // If modal open, Esc closes modal, Enter launches
      if (selectedGame) {
        if (e.key === 'Escape' || e.key === 'b' || e.key === 'B') {
          e.preventDefault();
          setSelectedGame(null);
        } else if (e.key === 'Enter' || e.key === 'a' || e.key === 'A') {
          e.preventDefault();
          const targetGame = selectedGame;
          setSelectedGame(null);
          setRunningGame(targetGame);
        } else if (e.key === 'f' || e.key === 'F') {
          e.preventDefault();
          toggleFavorite(selectedGame.id);
        }
        return;
      }

      if (e.key === 'Escape' || e.key === 'b' || e.key === 'B') {
        e.preventDefault();
        setCurrentView('home');
        return;
      }

      const activeTarget = focusedGame || games[0];

      // Console action buttons
      if (e.key === 'Enter' || e.key === 'a' || e.key === 'A') {
        if (currentView !== 'settings' && activeTarget) {
          e.preventDefault();
          soundManager.playLaunch();
          setRunningGame(activeTarget);
        }
      } else if (e.key === 'x' || e.key === 'X') {
        if (currentView !== 'settings' && activeTarget) {
          e.preventDefault();
          soundManager.playNavTick();
          setSelectedGame(activeTarget);
        }
      } else if (e.key === 'f' || e.key === 'F') {
        if (currentView !== 'settings' && activeTarget) {
          e.preventDefault();
          soundManager.playNavTick();
          toggleFavorite(activeTarget.id);
        }
      } else if (e.key === 's' || e.key === 'S' || e.key === 'y' || e.key === 'Y') {
        e.preventDefault();
        setCurrentView('consoles');
      } else if (e.key === 'h' || e.key === 'H') {
        e.preventDefault();
        setCurrentView('settings');
      } else if (currentView === 'settings') {
        const sections: Array<'theme' | 'cores' | 'controls' | 'sound'> = ['theme', 'cores', 'controls', 'sound'];
        const currentIdx = sections.indexOf(settingsTab);
        if (e.key === 'q' || e.key === 'Q') {
          e.preventDefault();
          soundManager.playNavTick();
          const prevIdx = (currentIdx - 1 + sections.length) % sections.length;
          setSettingsTab(sections[prevIdx]);
        } else if (e.key === 'e' || e.key === 'E') {
          e.preventDefault();
          soundManager.playNavTick();
          const nextIdx = (currentIdx + 1) % sections.length;
          setSettingsTab(sections[nextIdx]);
        }
      }
    };

    window.addEventListener('keydown', handleGlobalKeyDown);
    return () => window.removeEventListener('keydown', handleGlobalKeyDown);
  }, [
    runningGame, 
    selectedGame, 
    games, 
    hoveredGame,
    focusedGame,
    focusedIndex, 
    setFocusedIndex, 
    setSelectedGame, 
    setRunningGame, 
    toggleFavorite,
    setCurrentView,
    currentView,
    settingsTab,
    setSettingsTab
  ]);

  return (
    <div className={`relative h-screen w-full flex flex-col text-slate-100 overflow-hidden select-none selection:bg-white/20 selection:text-white ${keyboardEnabled ? '' : 'pointer-events-none'}`}>
      {/* Background layer */}
      <GlassBackground />

      {/* Input source indicator */}
      <div className="absolute top-3 right-4 z-40 px-3 py-1.5 rounded-xl glass-pill text-[10px] font-mono tracking-wide text-slate-200">
        {connected ? 'WII U GAMEPAD' : 'CONECTA UN MANDO'}
      </div>

      {/* Main Viewport Content - Smoothly scrollable with dedicated scroll container */}
      <main className="relative z-10 flex-1 w-full max-w-7xl mx-auto px-4 sm:px-6 py-6 overflow-y-auto overflow-x-hidden">
        {currentView === 'home' && <HomeView />}
        {currentView === 'consoles' && <ConsolesView />}
        {currentView === 'settings' && <SettingsView />}
      </main>

      {/* Bottom Console Navigation Bar */}
      <div className="shrink-0">
        <BottomNavBar />
      </div>

      {/* Detail Game Modal */}
      {selectedGame && (
        <GameModal
          game={selectedGame}
          onClose={() => setSelectedGame(null)}
          onPlay={() => {
            const gameToPlay = selectedGame;
            setSelectedGame(null);
            setRunningGame(gameToPlay);
          }}
        />
      )}

      {/* Running Interactive Emulator Simulation */}
      {runningGame && (
        <EmulatorModal
          game={runningGame}
          onClose={() => setRunningGame(null)}
        />
      )}
    </div>
  );
};

export default function App() {
  return (
    <RetroProvider>
      <MainShell />
    </RetroProvider>
  );
}
