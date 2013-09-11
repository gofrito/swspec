% Reference:
% http://cellspe-tasklib.cvs.sourceforge.net/*checkout*/cellspe-tasklib/cellspe-tasklib/spectrometer/doc/SWspectrometer.pdf?revision=1.1

% -- Test vector

clear;
Np = 1024;  % FFT segment length
jp = 0 : (Np-1);
Nt = Np*Np; % A couple of FFT segments
jt = 0 : (Nt-1);

noise  = randn([1 Nt]); % Gaussian noise
fcarr  = 1/37;          % Set 37 bins period
signal = noise + 0.1*cos(2*pi*fcarr*jt); % Test wave plus noise
freqs  = jp ./ Np;

% -- FFT preparations

%winf     = ones([Np 1]);
winf     = cos((jp - Np/2) * pi/Np)'; % Simple cos windowing function
%winf     = 0.53836 - 0.46164*cos(2*pi*jp./(Np-1))'; % Hamming
segments = buffer(signal,Np,Np/2);    % Segments with 50% overlap

Ns   = size(segments,2);
js   = 0 : (Ns-1);

% -- Process

powspect = zeros([Np 1]);
intspect = zeros([Np 1]);
for i = 1 : Ns
    spect = fft(segments(:,i).*winf);
    intspect = intspect + (1/Ns)*spect;
    powspect = powspect + (1/Ns)*(spect .* conj(spect));
end

% -- Results

%figure(1), clf, plot(winf), title('Windowing function'), grid on;
figure(2), clf, plot(freqs, powspect), title('Full band power spectrum'), grid on;
%figure(3), clf,
%    subplot(2,1,1), loglog(jp, abs(intspect)), ylabel('Mag'), grid on,
%    title('Integrated complex spectrum'),
%    subplot(2,1,2), semilogx(jp, angle(intspect)), ylabel('Phase'), grid on;
%figure(4), clf, plot3(jp,real(intspect),imag(intspect),'g-.'), title('Fancy integrated complex spectrum'), grid on;

[pk,ipk]=max(powspect(1:(Np/2)));
fprintf(1, 'Peak %f at %f, preset carrier %f\n', pk, freqs(ipk), fcarr);

% overlap/shuffle checking:
% N=1024;
% data=randn(1,3*N) + 5*cos(2*pi*0.5*(0:3*N-1)); fdata=fft(data);
% sdata=[data(2*N:3*N) data(1:2*N-1)]; fsdata=fft(sdata);
% figure(1), subplot(3,1,1), plot(abs(fdata)), subplot(3,1,2), plot(abs(fsdata)), subplot(3,1,3), plot(abs(fsdata) - abs(fdata));
