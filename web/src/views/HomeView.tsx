import React, { useState, useEffect, useCallback } from 'react';
import { 
  Star, 
  Clock, 
  Gamepad2, 
  ArrowRight
} from 'lucide-react';
import { useRetro } from '../context/RetroContext';
import { GameCard } from '../components/GameCard';
import { soundManager } from '../utils/audio';

type HomeSection = 'favorites' | 'recents' | 'all';

export const HomeView: React.FC = () => {
  const { 
    games, 
    activeAccent,
    setSelectedGame, 
    setRunningGame, 
    setCurrentView,
    setSelectedConsole,
    setHoveredGame,
    hoveredGame,
    setFocusedGame,
    runningGame,
    selectedGame
  } = useRetro();

  const favoriteGames = games.filter(g => g.isFavorite);
  const favoriteSlice = favoriteGames.slice(0, 5);
  const recentGames = [...games].sort((a, b) => (b.launchCount || 0) - (a.launchCount || 0)).slice(0, 4);

  // Spatial navigation state across sections (favorites, recents, all)
  const [homeSection, setHomeSection] = useState<HomeSection>(
    favoriteSlice.length > 0 ? 'favorites' : (recentGames.length > 0 ? 'recents' : 'all')
  );
  const [homeIndex, setHomeIndex] = useState<number>(0);

  // Compute the currently focused game object
  const getCurrentGame = useCallback((): typeof games[0] => {
    if (homeSection === 'favorites' && favoriteSlice.length > 0) {
      return favoriteSlice[Math.min(homeIndex, favoriteSlice.length - 1)] || games[0];
    }
    if (homeSection === 'recents' && recentGames.length > 0) {
      return recentGames[Math.min(homeIndex, recentGames.length - 1)] || games[0];
    }
    return games[Math.min(homeIndex, games.length - 1)] || games[0];
  }, [homeSection, homeIndex, favoriteSlice, recentGames, games]);

  // Sync focused game into RetroContext so BottomNavBar and GlassBackground receive it
  useEffect(() => {
    const active = getCurrentGame();
    if (active) {
      setFocusedGame(active);
    }
  }, [getCurrentGame, setFocusedGame]);

  // Keyboard spatial navigation across sections
  useEffect(() => {
    // If modal or emulator is active, don't intercept
    if (selectedGame || runningGame) return;

    const handleHomeKeyDown = (e: KeyboardEvent) => {
      // Don't intercept if typing in an input
      const target = e.target as HTMLElement;
      if (target.tagName === 'INPUT' || target.tagName === 'SELECT' || target.tagName === 'TEXTAREA') {
        return;
      }

      if (e.key === 'ArrowDown') {
        e.preventDefault();
        soundManager.playNavTick();
        setHoveredGame(null);

        if (homeSection === 'favorites') {
          if (recentGames.length > 0) {
            setHomeSection('recents');
            setHomeIndex(Math.min(homeIndex, recentGames.length - 1));
          } else {
            setHomeSection('all');
            setHomeIndex(0);
          }
        } else if (homeSection === 'recents') {
          setHomeSection('all');
          setHomeIndex(Math.min(homeIndex, Math.min(3, games.length - 1)));
        } else if (homeSection === 'all') {
          // Move down 4 items in the all games grid
          setHomeIndex(prev => Math.min(games.length - 1, prev + 4));
        }
      } else if (e.key === 'ArrowUp') {
        e.preventDefault();
        soundManager.playNavTick();
        setHoveredGame(null);

        if (homeSection === 'all') {
          if (homeIndex < 4) {
            if (recentGames.length > 0) {
              setHomeSection('recents');
              setHomeIndex(Math.min(homeIndex, recentGames.length - 1));
            } else if (favoriteSlice.length > 0) {
              setHomeSection('favorites');
              setHomeIndex(0);
            }
          } else {
            setHomeIndex(prev => Math.max(0, prev - 4));
          }
        } else if (homeSection === 'recents') {
          if (favoriteSlice.length > 0) {
            setHomeSection('favorites');
            setHomeIndex(Math.min(homeIndex, favoriteSlice.length - 1));
          }
        }
      } else if (e.key === 'ArrowRight') {
        e.preventDefault();
        soundManager.playNavTick();
        setHoveredGame(null);

        if (homeSection === 'favorites') {
          setHomeIndex(prev => Math.min(favoriteSlice.length - 1, prev + 1));
        } else if (homeSection === 'recents') {
          setHomeIndex(prev => Math.min(recentGames.length - 1, prev + 1));
        } else if (homeSection === 'all') {
          setHomeIndex(prev => Math.min(games.length - 1, prev + 1));
        }
      } else if (e.key === 'ArrowLeft') {
        e.preventDefault();
        soundManager.playNavTick();
        setHoveredGame(null);

        if (homeSection === 'favorites') {
          setHomeIndex(prev => Math.max(0, prev - 1));
        } else if (homeSection === 'recents') {
          setHomeIndex(prev => Math.max(0, prev - 1));
        } else if (homeSection === 'all') {
          setHomeIndex(prev => Math.max(0, prev - 1));
        }
      }
    };

    window.addEventListener('keydown', handleHomeKeyDown);
    return () => window.removeEventListener('keydown', handleHomeKeyDown);
  }, [
    homeSection, 
    homeIndex, 
    favoriteSlice.length, 
    recentGames.length, 
    games.length, 
    selectedGame, 
    runningGame, 
    setHoveredGame
  ]);

  return (
    <div className="space-y-8 pb-12 animate-in fade-in duration-300">
      {/* Favorites Section */}
      {favoriteSlice.length > 0 && (
        <div className="space-y-4">
          <div className="flex items-center justify-between">
            <div className="flex items-center gap-2">
              <Star className="w-4 h-4 text-amber-400 fill-current" />
              <h2 className="text-lg font-bold text-white tracking-tight">
                Favoritos
              </h2>
              <span className="text-xs font-mono text-slate-400">({favoriteGames.length})</span>
            </div>

            <button
              onClick={() => {
                setSelectedConsole('all');
                setCurrentView('consoles');
              }}
              className="text-xs font-medium text-slate-400 hover:text-white flex items-center gap-1 transition-colors cursor-pointer"
            >
              <span>Ver catálogo completo</span>
              <ArrowRight className="w-3.5 h-3.5" />
            </button>
          </div>

          <div className="grid grid-cols-2 sm:grid-cols-3 md:grid-cols-4 lg:grid-cols-5 gap-4">
            {favoriteSlice.map((game, idx) => {
              const isFocused = !hoveredGame && homeSection === 'favorites' && homeIndex === idx;
              return (
                <GameCard
                  key={game.id}
                  game={game}
                  isFocused={isFocused}
                  onPlay={() => {
                    soundManager.playLaunch();
                    setRunningGame(game);
                  }}
                  onDetail={() => {
                    soundManager.playNavTick();
                    setSelectedGame(game);
                  }}
                />
              );
            })}
          </div>
        </div>
      )}

      {/* Recents Section */}
      {recentGames.length > 0 && (
        <div className="space-y-4">
          <div className="flex items-center gap-2">
            <Clock className="w-4 h-4" style={{ color: activeAccent.hex }} />
            <h2 className="text-lg font-bold text-white tracking-tight">
              Jugados Recientemente
            </h2>
          </div>

          <div className="grid grid-cols-2 sm:grid-cols-3 md:grid-cols-4 gap-4">
            {recentGames.map((game, idx) => {
              const isFocused = !hoveredGame && homeSection === 'recents' && homeIndex === idx;
              return (
                <GameCard
                  key={game.id}
                  game={game}
                  isFocused={isFocused}
                  onPlay={() => {
                    soundManager.playLaunch();
                    setRunningGame(game);
                  }}
                  onDetail={() => {
                    soundManager.playNavTick();
                    setSelectedGame(game);
                  }}
                />
              );
            })}
          </div>
        </div>
      )}

      {/* All Games Grid */}
      <div className="space-y-4">
        <div className="flex items-center justify-between">
          <div className="flex items-center gap-2">
            <Gamepad2 className="w-4 h-4 text-slate-400" />
            <h2 className="text-lg font-bold text-white tracking-tight">
              Toda la Colección
            </h2>
            <span className="text-xs font-mono text-slate-400">({games.length} títulos)</span>
          </div>
        </div>

        <div className="grid grid-cols-2 sm:grid-cols-3 md:grid-cols-4 lg:grid-cols-6 gap-4">
          {games.map((game, idx) => {
            const isFocused = !hoveredGame && homeSection === 'all' && homeIndex === idx;
            return (
              <GameCard
                key={game.id}
                game={game}
                isFocused={isFocused}
                onPlay={() => {
                  soundManager.playLaunch();
                  setRunningGame(game);
                }}
                onDetail={() => {
                  soundManager.playNavTick();
                  setSelectedGame(game);
                }}
              />
            );
          })}
        </div>
      </div>
    </div>
  );
};
