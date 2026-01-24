const meters = document.querySelectorAll("[data-depth-meter]");

function updateMeters() {
  const docHeight = document.documentElement.scrollHeight - window.innerHeight;
  const progress = docHeight > 0 ? window.scrollY / docHeight : 0;
  const altitude = Math.round(5000 - progress * 10000);

  meters.forEach((meter) => {
    meter.textContent = `${altitude}m`;
    meter.style.setProperty("--progress", progress.toString());
  });
}

if (meters.length > 0) {
  updateMeters();
  window.addEventListener("scroll", updateMeters, { passive: true });
  window.addEventListener("resize", updateMeters);
}

const audio = document.querySelector("[data-synesthetic-audio]");
const synesthetic = document.querySelector("[data-synesthetic]");

if (audio && synesthetic) {
  const sun = synesthetic.querySelector(".synesthetic__sun");
  const rabbit = synesthetic.querySelector(".synesthetic__rabbit");
  const wave = synesthetic.querySelector(".synesthetic__wave");
  let analyser;
  let dataArray;
  let rafId;

  const audioContext = new (window.AudioContext || window.webkitAudioContext)();
  const source = audioContext.createMediaElementSource(audio);
  analyser = audioContext.createAnalyser();
  analyser.fftSize = 256;
  dataArray = new Uint8Array(analyser.frequencyBinCount);
  source.connect(analyser);
  analyser.connect(audioContext.destination);

  const animate = () => {
    analyser.getByteFrequencyData(dataArray);
    const avg = dataArray.reduce((sum, value) => sum + value, 0) / dataArray.length || 0;
    const energy = Math.min(avg / 255, 1);
    const progress = audio.duration ? audio.currentTime / audio.duration : 0;

    if (sun) {
      sun.style.transform = `translateY(${60 - progress * 120}%)`;
      sun.style.opacity = `${0.4 + progress * 0.6}`;
    }

    if (rabbit) {
      rabbit.style.transform = `translate(-50%, ${30 - progress * 60 - energy * 10}%)`;
      rabbit.style.opacity = `${0.5 + energy * 0.5}`;
    }

    if (wave) {
      wave.style.transform = `scaleY(${0.8 + energy * 0.6})`;
    }

    rafId = window.requestAnimationFrame(animate);
  };

  audio.addEventListener("play", () => {
    audioContext.resume();
    if (!rafId) {
      rafId = window.requestAnimationFrame(animate);
    }
  });

  audio.addEventListener("pause", () => {
    if (rafId) {
      window.cancelAnimationFrame(rafId);
      rafId = null;
    }
  });

  audio.addEventListener("ended", () => {
    if (rafId) {
      window.cancelAnimationFrame(rafId);
      rafId = null;
    }
  });
}
