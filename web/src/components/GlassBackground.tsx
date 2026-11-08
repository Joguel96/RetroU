import React from 'react';
import { useRetro } from '../context/RetroContext';
import { CONSOLES } from '../data/mockData';

export const GlassBackground: React.FC = () => {
  const { theme, activeAccent, hoveredGame, focusedGame, games, focusedIndex } = useRetro();

  // If a game is hovered or focused via keyboard, activate dynamic PS3/PS4 backdrop
  const activeGame = hoveredGame || focusedGame || (games[focusedIndex] ? games[focusedIndex] : null);
  const activeConsoleInfo = activeGame ? CONSOLES.find(c => c.id === activeGame.consoleId) : null;

  return (
    <div className="fixed inset-0 pointer-events-none select-none z-0 overflow-hidden transition-all duration-700">
      {/* 1. Base Default Wallpaper (Always present beneath) */}
      <div className="absolute inset-0 bg-slate-950">
        {theme.wallpaperId === 'slate-minimal' && (
          <div className="absolute inset-0 bg-gradient-to-br from-slate-900 via-slate-950 to-neutral-950" />
        )}

        {theme.wallpaperId === 'titanium-carbon' && (
          <div className="absolute inset-0 bg-neutral-950">
            <div 
              className="absolute inset-0 opacity-10"
              style={{
                backgroundImage: 'radial-gradient(circle at 2px 2px, rgba(255,255,255,0.2) 1px, transparent 0)',
                backgroundSize: '20px 20px',
              }}
            />
            <div className="absolute inset-0 bg-gradient-to-b from-transparent via-slate-950/70 to-black" />
          </div>
        )}

        {theme.wallpaperId === 'deep-cosmos' && (
          <div className="absolute inset-0 bg-gradient-to-tr from-slate-950 via-indigo-950/70 to-slate-900">
            <div className="absolute -top-32 right-1/4 w-[600px] h-[600px] rounded-full bg-indigo-600/10 blur-[130px]" />
            <div className="absolute bottom-10 left-10 w-[550px] h-[550px] rounded-full bg-slate-800/20 blur-[140px]" />
          </div>
        )}

        {theme.wallpaperId === 'aurora-boreal' && (
          <div className="absolute inset-0 bg-gradient-to-br from-teal-950/60 via-slate-950 to-neutral-950">
            <div className="absolute top-1/4 left-1/3 w-[700px] h-[400px] bg-emerald-700/10 blur-[130px]" />
          </div>
        )}

        {theme.wallpaperId === 'pure-studio' && (
          <div className="absolute inset-0 bg-gradient-to-b from-neutral-900 via-slate-950 to-zinc-950" />
        )}
      </div>

      {/* 2. DYNAMIC PS3 / PS4 GAME WALLPAPER BACKDROP LAYER */}
      {/* Crossfades smoothly when the cursor hovers or selects a ROM game */}
      {games.map(game => {
        const isGameActive = activeGame?.id === game.id;
        return (
          <div
            key={game.id}
            className={`absolute inset-0 transition-opacity duration-700 ease-out ${
              isGameActive ? 'opacity-100' : 'opacity-0'
            }`}
          >
            {/* Thematic Cinematic Gradient Art */}
            <div 
              className="absolute inset-0"
              style={{
                background: `radial-gradient(ellipse 90% 80% at 75% 30%, ${game.backdropGradient[0]} 0%, ${game.backdropGradient[1]} 55%, ${game.backdropGradient[2]} 100%)`,
              }}
            />

            {/* Cinematic Lighting Orbs for the game atmosphere */}
            <div 
              className="absolute -top-20 right-10 w-[650px] h-[650px] rounded-full blur-[120px] opacity-40"
              style={{ backgroundColor: game.coverGradient[0] }}
            />
            <div 
              className="absolute bottom-10 left-1/4 w-[500px] h-[500px] rounded-full blur-[140px] opacity-25"
              style={{ backgroundColor: game.coverGradient[1] }}
            />

            {/* Giant Watermark / Emblem Silhouette on the Right (Like PS4 Dynamic Themes) */}
            <div className="absolute right-8 top-1/4 -translate-y-12 select-none pointer-events-none opacity-[0.06] transform scale-150 rotate-3 text-[180px] sm:text-[260px] filter blur-[1px]">
              {game.coverIcon}
            </div>

            {/* Subtle PS3/PS4 HUD atmospheric info ticker in the backdrop top-right */}
            <div className="absolute top-20 right-10 z-10 text-right opacity-30 hidden lg:block">
              <span className="text-[10px] font-mono tracking-widest uppercase text-slate-300 block">
                {activeConsoleInfo?.fullName}
              </span>
              <span className="text-xs font-serif italic text-slate-200 mt-1 block max-w-sm">
                "{game.backdropQuote}"
              </span>
            </div>
          </div>
        );
      })}

      {/* 3. Deep Cinematic Vignette & Readability Scrim (Ensures 100% WCAG AA contrast for text) */}
      <div className="absolute inset-0 bg-gradient-to-t from-slate-950 via-slate-950/75 to-slate-950/60" />
      <div className="absolute inset-0 shadow-[inset_0_0_120px_rgba(0,0,0,0.85)]" />

      {/* 4. Subtle frosted top glass hairline */}
      <div className="absolute top-0 inset-x-0 h-[1px] bg-gradient-to-r from-transparent via-white/20 to-transparent" />

      {/* 5. Optional CRT effect */}
      {theme.crtEffect && (
        <div className="crt-scanlines absolute inset-0 z-50 opacity-30" />
      )}
    </div>
  );
};
