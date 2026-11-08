import React, { useState, useEffect } from 'react';
import { 
  Search, 
  Layers, 
  Cpu, 
  Gamepad2, 
  Sparkles, 
  X, 
  RefreshCw, 
  HardDrive, 
  StopCircle, 
  FolderCheck, 
  CheckCircle2 
} from 'lucide-react';
import { useRetro } from '../context/RetroContext';
import { GameCard } from '../components/GameCard';
import { CONSOLES } from '../data/mockData';
import { soundManager } from '../utils/audio';

export const ConsolesView: React.FC = () => {
  const { 
    games, 
    selectedConsole, 
    setSelectedConsole, 
    setSelectedGame, 
    setRunningGame,
    activeAccent,
    scanState,
    startScan,
    cancelScan,
    showScanModal,
    setShowScanModal,
    hoveredGame,
    setHoveredGame,
    setFocusedGame,
    runningGame,
    selectedGame
  } = useRetro();

  const [search, setSearch] = useState('');
  const [selectedGenre, setSelectedGenre] = useState<string>('all');
  const [focusedIndex, setFocusedIndex] = useState<number>(0);

  // Available genres
  const genres = ['all', 'Plataformas', 'JRPG', 'Metroidvania / Acción', 'Lucha 2D', 'Lucha 3D', 'Aventura / RPG'];

  // Current platform info
  const currentConsoleInfo = CONSOLES.find(c => c.id === selectedConsole);

  // Dynamic directory and label for the scan button
  const currentDirectory = selectedConsole === 'all' 
    ? '/sdcard/roms/' 
    : currentConsoleInfo?.romDirectory || `/sdcard/roms/${selectedConsole}/`;

  const scanButtonLabel = selectedConsole === 'all'
    ? 'Escanear Todo (/sdcard/roms/)'
    : `Escanear ${currentConsoleInfo?.name || selectedConsole.toUpperCase()} (${currentDirectory})`;

  // Filtered games
  const filteredGames = games.filter(g => {
    const matchesConsole = selectedConsole === 'all' || g.consoleId === selectedConsole;
    const matchesSearch = search === '' || 
      g.title.toLowerCase().includes(search.toLowerCase()) || 
      g.developer.toLowerCase().includes(search.toLowerCase());
    const matchesGenre = selectedGenre === 'all' || g.genre.toLowerCase().includes(selectedGenre.toLowerCase());
    return matchesConsole && matchesSearch && matchesGenre;
  });

  // Keep focusedIndex within range when filteredGames changes
  useEffect(() => {
    if (focusedIndex >= filteredGames.length) {
      setFocusedIndex(Math.max(0, filteredGames.length - 1));
    }
  }, [filteredGames.length, focusedIndex]);

  // Synchronize focusedGame so BottomNavBar and GlassBackground update immediately
  useEffect(() => {
    if (filteredGames.length > 0 && filteredGames[focusedIndex]) {
      setFocusedGame(filteredGames[focusedIndex]);
    }
  }, [filteredGames, focusedIndex, setFocusedGame]);

  // Keyboard navigation for Consoles view
  useEffect(() => {
    if (selectedGame || runningGame) return;

    const handleKeyDown = (e: KeyboardEvent) => {
      const target = e.target as HTMLElement;
      if (target.tagName === 'INPUT' || target.tagName === 'SELECT' || target.tagName === 'TEXTAREA') {
        return;
      }

      if (filteredGames.length === 0) return;

      if (e.key === 'ArrowRight') {
        e.preventDefault();
        soundManager.playNavTick();
        setHoveredGame(null);
        setFocusedIndex(prev => Math.min(filteredGames.length - 1, prev + 1));
      } else if (e.key === 'ArrowLeft') {
        e.preventDefault();
        soundManager.playNavTick();
        setHoveredGame(null);
        setFocusedIndex(prev => Math.max(0, prev - 1));
      } else if (e.key === 'ArrowDown') {
        e.preventDefault();
        soundManager.playNavTick();
        setHoveredGame(null);
        setFocusedIndex(prev => Math.min(filteredGames.length - 1, prev + 4));
      } else if (e.key === 'ArrowUp') {
        e.preventDefault();
        soundManager.playNavTick();
        setHoveredGame(null);
        setFocusedIndex(prev => Math.max(0, prev - 4));
      }
    };

    window.addEventListener('keydown', handleKeyDown);
    return () => window.removeEventListener('keydown', handleKeyDown);
  }, [filteredGames.length, selectedGame, runningGame, setHoveredGame]);

  return (
    <div className="space-y-6 pb-12 animate-in fade-in duration-300">
      {/* Platform Shelf Header */}
      <div className="flex flex-col md:flex-row md:items-center justify-between gap-4">
        <div>
          <h2 className="text-xl font-bold text-white flex items-center gap-2">
            <Layers className="w-5 h-5 text-slate-300" />
            <span>Biblioteca por Plataforma</span>
          </h2>
          <p className="text-xs text-slate-400 mt-0.5">
            Explora por consola o escanea el almacenamiento local de forma selectiva.
          </p>
        </div>

        {/* Dynamic Contextual Scan Button */}
        <div className="flex items-center gap-3">
          {scanState.isScanning ? (
            <button
              onClick={cancelScan}
              className="flex items-center gap-2 px-4 py-2 rounded-xl text-xs font-semibold bg-rose-500/20 hover:bg-rose-500/30 border border-rose-500/30 text-rose-200 transition-colors cursor-pointer"
            >
              <StopCircle className="w-4 h-4" />
              <span>Detener Escaneo ({scanState.progress}%)</span>
            </button>
          ) : (
            <button
              onClick={() => startScan(selectedConsole)}
              className="flex items-center gap-2 px-4 py-2.5 rounded-xl text-xs font-semibold text-slate-900 bg-white hover:bg-slate-100 shadow-xl transition-all active:scale-95 cursor-pointer"
              style={{
                boxShadow: 'inset 0 1px 1px rgba(255,255,255,0.4), 0 8px 20px rgba(0,0,0,0.4)',
              }}
            >
              <RefreshCw className="w-3.5 h-3.5" />
              <span>{scanButtonLabel}</span>
            </button>
          )}
        </div>
      </div>

      {/* Platform Shelf Selector */}
      <div className="grid grid-cols-2 sm:grid-cols-3 md:grid-cols-5 lg:grid-cols-9 gap-2.5">
        {/* "ALL" option */}
        <button
          onClick={() => setSelectedConsole('all')}
          className={`flex flex-col items-center justify-center p-3 rounded-2xl glass-card transition-all cursor-pointer ${
            selectedConsole === 'all'
              ? 'bg-white/20 border-white/40 shadow-xl ring-1 ring-white/30'
              : 'hover:bg-white/10'
          }`}
        >
          <Sparkles className="w-5 h-5 mb-1.5 text-slate-200" />
          <span className="text-xs font-bold text-white">Todo</span>
          <span className="text-[10px] font-mono text-slate-300 mt-0.5">{games.length} ROMs</span>
        </button>

        {CONSOLES.map(c => {
          const isSelected = selectedConsole === c.id;
          const count = games.filter(g => g.consoleId === c.id).length;
          return (
            <button
              key={c.id}
              onClick={() => setSelectedConsole(c.id)}
              className={`flex flex-col items-center justify-center p-3 rounded-2xl glass-card transition-all cursor-pointer ${
                isSelected
                  ? 'bg-white/20 border-white/40 shadow-xl ring-1 ring-white/30'
                  : 'hover:bg-white/10'
              }`}
            >
              <div 
                className={`w-8 h-8 rounded-xl flex items-center justify-center mb-1.5 text-xs font-bold font-mono transition-colors ${
                  isSelected 
                    ? 'bg-white text-slate-950 shadow-md' 
                    : 'glass-pill text-white'
                }`}
              >
                {c.name.slice(0, 3)}
              </div>
              <span className="text-xs font-bold text-white truncate max-w-[80px]">
                {c.name}
              </span>
              <span className="text-[10px] font-mono text-slate-300 mt-0.5">
                {count} ROMs
              </span>
            </button>
          );
        })}
      </div>

      {/* Platform Info Banner */}
      <div className="p-4 rounded-2xl glass-panel border border-white/15 flex flex-wrap items-center justify-between gap-4">
        <div className="flex items-center gap-3">
          <div className="px-3 py-1.5 rounded-xl text-xs font-mono font-bold tracking-wider uppercase text-white bg-white/10 border border-white/15">
            {currentConsoleInfo ? currentConsoleInfo.name : 'TODAS'}
          </div>
          <div>
            <h3 className="text-sm font-bold text-white">
              {currentConsoleInfo ? `${currentConsoleInfo.fullName} (${currentConsoleInfo.year})` : 'Colección General RetroU'}
            </h3>
            <p className="text-xs text-slate-400">
              Directorio de ROMs fijo: <code className="text-slate-300 font-mono">{currentDirectory}</code>
            </p>
          </div>
        </div>

        <div className="flex items-center gap-4 text-xs font-mono text-slate-300">
          {currentConsoleInfo && (
            <div className="flex items-center gap-1.5">
              <Cpu className="w-3.5 h-3.5 text-slate-400" />
              <span>Núcleo: <strong className="text-white">{currentConsoleInfo.core}</strong></span>
            </div>
          )}
          <div className="flex items-center gap-1.5">
            <HardDrive className="w-3.5 h-3.5 text-slate-400" />
            <span>Ext: <strong className="text-white">{currentConsoleInfo ? currentConsoleInfo.romExtensions.join(', ') : 'todas'}</strong></span>
          </div>
        </div>
      </div>

      {/* Filter and Search Bar */}
      <div className="flex flex-col sm:flex-row items-stretch sm:items-center justify-between gap-3">
        <div className="relative flex-1 max-w-md">
          <Search className="absolute left-3.5 top-1/2 -translate-y-1/2 w-4 h-4 text-slate-400" />
          <input
            type="text"
            data-gamepad-ignore
            value={search}
            onChange={(e) => setSearch(e.target.value)}
            placeholder="Buscar por título o desarrollador..."
            className="w-full pl-10 pr-9 py-2 rounded-xl bg-white/5 border border-white/10 text-xs text-white placeholder-slate-400 focus:outline-none focus:ring-1 focus:ring-white/40 backdrop-blur-md"
          />
          {search && (
            <button
              onClick={() => setSearch('')}
              className="absolute right-3 top-1/2 -translate-y-1/2 text-slate-400 hover:text-white"
            >
              <X className="w-3.5 h-3.5" />
            </button>
          )}
        </div>

        {/* Genre Tags */}
        <div className="flex items-center gap-1.5 overflow-x-auto py-1">
          {genres.map(g => {
            const isGenreActive = selectedGenre === g;
            return (
              <button
                key={g}
                onClick={() => setSelectedGenre(g)}
                className={`px-3 py-1.5 text-xs font-medium rounded-lg whitespace-nowrap transition-colors ${
                  isGenreActive
                    ? 'bg-white/20 text-white font-semibold border border-white/25'
                    : 'text-slate-400 hover:text-white hover:bg-white/5'
                }`}
              >
                {g === 'all' ? 'Todos los géneros' : g}
              </button>
            );
          })}
        </div>
      </div>

      {/* Games Catalog Grid */}
      {filteredGames.length > 0 ? (
        <div className="grid grid-cols-2 sm:grid-cols-3 md:grid-cols-4 lg:grid-cols-6 gap-4">
          {filteredGames.map((game, idx) => (
            <GameCard
              key={game.id}
              game={game}
              isFocused={!hoveredGame && focusedIndex === idx}
              onPlay={() => {
                soundManager.playLaunch();
                setRunningGame(game);
              }}
              onDetail={() => {
                soundManager.playNavTick();
                setSelectedGame(game);
              }}
            />
          ))}
        </div>
      ) : (
        <div className="p-12 text-center rounded-3xl glass-panel border border-white/10 space-y-3">
          <Gamepad2 className="w-10 h-10 mx-auto text-slate-500" />
          <h3 className="text-base font-bold text-white">No hay ROMs registradas</h3>
          <p className="text-xs text-slate-400 max-w-sm mx-auto">
            No se encontraron archivos en {currentDirectory}. Pulsa "{scanButtonLabel}" para comprobar si hay nuevas ROMs en el almacenamiento.
          </p>
          <button
            onClick={() => startScan(selectedConsole)}
            className="px-4 py-2 rounded-xl text-xs font-semibold text-slate-900 bg-white hover:bg-slate-200 transition-colors"
          >
            Ejecutar Escaneo Ahora
          </button>
        </div>
      )}

      {/* Contextual Scanner Modal / Feedback Overlay */}
      {showScanModal && (
        <div className="fixed inset-0 z-50 flex items-center justify-center p-4 bg-black/75 backdrop-blur-md animate-in fade-in duration-200">
          <div className="w-full max-w-lg rounded-3xl glass-panel border border-white/20 p-6 space-y-4 shadow-2xl">
            <div className="flex items-center justify-between border-b border-white/10 pb-3">
              <div className="flex items-center gap-2">
                <FolderCheck className="w-5 h-5 text-slate-200" />
                <div>
                  <h3 className="text-sm font-bold text-white">
                    {scanState.isScanning ? 'Escaneando Directorio...' : 'Escaneo Concluido'}
                  </h3>
                  <span className="text-[11px] font-mono text-slate-400">
                    Ruta: {scanState.currentTarget}
                  </span>
                </div>
              </div>

              {!scanState.isScanning && (
                <button
                  onClick={() => setShowScanModal(false)}
                  className="p-1 rounded-lg hover:bg-white/10 text-slate-400 hover:text-white"
                >
                  <X className="w-4 h-4" />
                </button>
              )}
            </div>

            {/* Progress Bar */}
            <div className="space-y-2">
              <div className="flex items-center justify-between text-xs font-mono">
                <span className="text-slate-300 truncate max-w-xs">
                  {scanState.currentFile}
                </span>
                <span className="font-bold text-white">{scanState.progress}%</span>
              </div>

              <div className="w-full h-2.5 rounded-full bg-white/10 overflow-hidden relative">
                <div
                  className="h-full rounded-full bg-white transition-all duration-300"
                  style={{ width: `${scanState.progress}%` }}
                />
              </div>
            </div>

            {/* Real-time Logs Console */}
            <div className="h-44 overflow-y-auto rounded-xl bg-black/60 border border-white/10 p-3 font-mono text-[11px] space-y-1 shadow-inner">
              {scanState.logs.map(log => (
                <div key={log.id} className="flex items-start gap-1.5 leading-relaxed">
                  <span className="text-slate-500">[{log.time}]</span>
                  <span
                    className={
                      log.type === 'success'
                        ? 'text-emerald-400'
                        : log.type === 'warn'
                        ? 'text-amber-400'
                        : 'text-slate-300'
                    }
                  >
                    {log.text}
                  </span>
                </div>
              ))}
            </div>

            {/* Footer Buttons */}
            <div className="flex items-center justify-between pt-2">
              <span className="text-xs text-slate-400 font-mono">
                ROMs encontradas: <strong className="text-emerald-400">{scanState.foundCount}</strong>
              </span>

              {scanState.isScanning ? (
                <button
                  onClick={cancelScan}
                  className="px-4 py-2 rounded-xl text-xs font-semibold bg-rose-500/20 hover:bg-rose-500/30 text-rose-200 border border-rose-500/30"
                >
                  Cancelar
                </button>
              ) : (
                <button
                  onClick={() => setShowScanModal(false)}
                  className="px-5 py-2 rounded-xl text-xs font-bold text-slate-900 bg-white hover:bg-slate-200"
                >
                  Listo / Aceptar
                </button>
              )}
            </div>
          </div>
        </div>
      )}
    </div>
  );
};
