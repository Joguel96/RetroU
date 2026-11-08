import React from 'react';
import { Star, Play, Clock, Sparkles, Info } from 'lucide-react';
import { RomGame } from '../types';
import { useRetro } from '../context/RetroContext';
import { CONSOLES } from '../data/mockData';

interface GameCardProps {
  game: RomGame;
  isFocused?: boolean;
  onSelect?: () => void;
  onPlay: () => void;
  onDetail?: () => void;
}

export const GameCard: React.FC<GameCardProps> = ({
  game,
  isFocused = false,
  onSelect,
  onPlay,
  onDetail,
}) => {
  const { toggleFavorite } = useRetro();
  const consoleInfo = CONSOLES.find(c => c.id === game.consoleId);

  const formatPlayTime = (minutes: number) => {
    if (!minutes) return 'Nuevo';
    const hrs = Math.floor(minutes / 60);
    const mins = minutes % 60;
    return hrs > 0 ? `${hrs}h ${mins}m` : `${mins}m`;
  };

  const cardRef = React.useRef<HTMLDivElement>(null);

  React.useEffect(() => {
    if (isFocused && cardRef.current) {
      cardRef.current.scrollIntoView({ block: 'nearest', behavior: 'smooth' });
    }
  }, [isFocused]);

  return (
    <div
      ref={cardRef}
      data-game-id={game.id}
      data-gamepad-focusable
      onClick={onPlay}
      className={`group relative flex flex-col rounded-2xl overflow-hidden glass-card cursor-pointer ${
        isFocused ? 'is-focused ring-2 ring-white/80 scale-[1.03] shadow-2xl z-10' : ''
      }`}
    >
      {/* Cover Artwork Container */}
      <div 
        className="relative aspect-[4/3] w-full flex items-center justify-center overflow-hidden p-6 text-white"
        style={{
          background: `linear-gradient(135deg, ${game.coverGradient[0]} 0%, ${game.coverGradient[1]} 100%)`,
        }}
      >
        {/* Subtle geometric pattern overlay */}
        <div 
          className="absolute inset-0 opacity-15"
          style={{
            backgroundImage: 'radial-gradient(circle at 1px 1px, rgba(255,255,255,0.4) 1px, transparent 0)',
            backgroundSize: '16px 16px',
          }}
        />

        {/* Ambient Top Light Specular */}
        <div className="absolute top-0 inset-x-0 h-1/2 bg-gradient-to-b from-white/20 via-white/5 to-transparent pointer-events-none" />

        {/* Icon / Emblem */}
        <div className="relative z-10 text-5xl transform transition-transform duration-300 group-hover:scale-110 drop-shadow-md">
          {game.coverIcon}
        </div>

        {/* Console Badge (Top Left) */}
        <div className="absolute top-2.5 left-2.5 z-20 flex items-center gap-1.5 px-2.5 py-1 rounded-lg glass-pill text-[10px] font-mono font-semibold tracking-wider uppercase text-slate-100">
          <span>{consoleInfo?.name || game.consoleId.toUpperCase()}</span>
        </div>

        {/* Favorite Button (Top Right) */}
        <button
          onClick={(e) => {
            e.stopPropagation();
            toggleFavorite(game.id);
          }}
          className={`absolute top-2.5 right-2.5 z-20 p-1.5 rounded-lg transition-all cursor-pointer ${
            game.isFavorite 
              ? 'bg-amber-500/25 border border-amber-400 text-amber-300 shadow-md backdrop-blur-md' 
              : 'glass-button text-slate-400 hover:text-white'
          }`}
          title={game.isFavorite ? 'Quitar de favoritos' : 'Marcar como favorito'}
        >
          <Star className={`w-3.5 h-3.5 ${game.isFavorite ? 'fill-current' : ''}`} />
        </button>

        {/* Hover Quick "JUGAR" & "Detalle" Button Overlay */}
        <div className="absolute inset-0 z-20 flex items-center justify-center gap-2 bg-slate-950/65 backdrop-blur-[3px] opacity-0 group-hover:opacity-100 transition-opacity p-2">
          <button
            onClick={(e) => {
              e.stopPropagation();
              onPlay();
            }}
            className="flex items-center gap-1.5 px-3.5 py-2 rounded-xl text-xs font-bold text-slate-900 bg-white hover:bg-slate-100 shadow-xl transition-transform transform active:scale-95 cursor-pointer"
          >
            <Play className="w-3.5 h-3.5 fill-current" />
            <span>JUGAR</span>
          </button>

          {onDetail && (
            <button
              onClick={(e) => {
                e.stopPropagation();
                onDetail();
              }}
              title="Ver detalle del juego (X)"
              className="p-2 rounded-xl glass-button text-slate-200 hover:text-white transition-transform active:scale-95 cursor-pointer"
            >
              <Info className="w-4 h-4 text-sky-400" />
            </button>
          )}
        </div>

        {/* Featured ribbon if applicable */}
        {game.featured && (
          <div className="absolute bottom-2 left-2 flex items-center gap-1 text-[10px] font-semibold text-slate-100 glass-pill px-2.5 py-0.5">
            <Sparkles className="w-2.5 h-2.5 text-amber-300" />
            <span>Destacado</span>
          </div>
        )}
      </div>

      {/* Card Information Footer */}
      <div className="p-3.5 flex flex-col justify-between flex-1 gap-2 bg-slate-950/50 backdrop-blur-md border-t border-white/10">
        <div>
          <h3 className="font-semibold text-sm text-slate-100 line-clamp-1 group-hover:text-white transition-colors">
            {game.title}
          </h3>
          <div className="flex items-center gap-1.5 text-[11px] text-slate-400 mt-0.5">
            <span>{game.developer}</span>
            <span aria-hidden="true">·</span>
            <span>{game.releaseYear}</span>
          </div>
        </div>

        {/* Bottom row: Genre & Execution count */}
        <div className="flex items-center justify-between text-[11px] text-slate-400 pt-2 border-t border-white/5">
          <span className="truncate max-w-[110px] text-slate-300">{game.genre}</span>
          <div className="flex items-center gap-1 text-[10px] font-mono text-slate-300">
            <span className="text-emerald-400 font-semibold">{game.launchCount || 0}</span>
            <span>ejec.</span>
          </div>
        </div>
      </div>
    </div>
  );
};
