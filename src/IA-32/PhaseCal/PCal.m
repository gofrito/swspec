%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%%%  Testbed for Phase Cal extraction                     %%%
%%%  (C) 2008 Jan Wagner, Metsähovi Radio Observatory     %%%
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
clear;

%% parameters of the test signal
fs = 32e6;
fspacing = 1e6; % PCal RF spacing, also try 5e6 for 5MHz
foffset = 10e3; % PCal baseband offset, also try 400e3 for 400kHz
N = 20e4;
w_phaseshift = 1e-5;

%% generate noisy test signal with as many tones as fit the bandwidth
sig = randn([1 N]);
Ntones = floor((fs/2)/fspacing);
if ((foffset + Ntones*fspacing) < fs/2),
   Ntones = Ntones+1;
end
xx = 0:(N-1);
xf = xx * fs/N;
for i=1:Ntones,
   fn = (foffset + (i-1)*fspacing)/fs;
   sig = sig + sin(2*pi*fn*xx + w_phaseshift);
end

%% filter for the bandshape
[fN,fWn] = buttord(0.65, 0.80, 6, 20); 
[fb,fa] = butter(fN,fWn);
sig = filter(fb, fa, sig);

figure(1), clf,
subplot(2,1,1), plot(sig), ylabel('Test signal'),
subplot(2,1,2), semilogy(xf, abs(fft(sig))), 
   axis tight, ylabel('Test signal');

%% extract pcal
if (foffset ~= 0),
    % rotate back into place
    w_off = -2*pi*foffset/fs;
    if (~1),
        % 'oscillator'
        r_sig = complex(sig.*cos(w_off*xx), sig.*sin(w_off*xx));
    else
        % precomputed rotator
        Nrvec = fs / gcd(fs, abs(foffset));
        fprintf(1, 'Precomputed length = %d (%.2f kB)\n', Nrvec, 2*4*Nrvec/2^10);
        rvec = complex(cos(w_off*xx(1:Nrvec)), sin(w_off*xx(1:Nrvec)));
        % now rotate
        Ntrunc = N - mod(N,Nrvec);
        r_sig = sig(1:Ntrunc) .* repmat(rvec, [1 Ntrunc/Nrvec]);
        r_sig = [r_sig zeros([1 mod(N,Nrvec)])]; % pad so that Matlab doesn't complain
    end
else
    % no rotation necessary
    r_sig = sig;
end
Nbins = fs/gcd(fs,fspacing);
Ntrunc = N - mod(N,Nbins);
pcal = sum(reshape(r_sig(1:Ntrunc), [Nbins Ntrunc/Nbins]), 2);
pcalf = fft(pcal);

%% plot the extracted pcal
pn = 0:(Nbins-1);
pf = fs*pn/Nbins;
pstep = round((fspacing/fs) * Nbins);
ptnums = pn/pstep;
pcalfmag = abs(pcalf);
figure(2), clf,
subplot(3,1,1), semilogy(xf, abs(fft(r_sig))), grid on, axis tight,
   hold on, semilogy(pf, abs(fft(pcal)), 'g-.'),
   ylabel('Logarithmic'), xlabel('Frequency'), 
   title('Rotated input signal with extracted PCal overlay'),
subplot(3,1,2), hold on, plot(ptnums, pcalfmag,'r-'), axis tight,
   stem(ptnums, pcalfmag), grid on,
   ylabel('Linear'), xlabel('Bin number rescaled to tone number'), 
   title('Extracted PCal in tone bins'),
subplot(3,1,3), plot(abs(pcal)), axis tight,
   ylabel('Linear'), xlabel('Time delay [s]'), 
   title('Extracted PCal in time-domain');

%% for "sparse" tones do a little bit of compressing

% slightly problematic in time domain, we may need fractional indices
% i.e. interpolation to get the folding right
% easier to just copy the freq domain bins
idx_tight = 1 + pstep*(0:(Ntones-1));
pcalf_tight = pcalf(idx_tight);
pf_tight = pf(idx_tight);
ptnums_tight = (1:Ntones)-1;
figure(3), clf,
subplot(2,1,1), hold on, 
   plot(ptnums, abs(real(pcalf)),'r-'),
   stem(ptnums, abs(real(pcalf)),'r'), 
   plot(ptnums, abs(imag(pcalf)),'g-'),
   stem(ptnums, abs(imag(pcalf)),'g'), 
   axis tight, grid on, 
   legend('abs(real(bin))','abs(real(bin))','abs(imag(bin))','abs(imag(bin))'),
   ylabel('Linear'), xlabel('Bin number rescaled to tone number'), 
   title('Extracted PCal in tone bins - complex'),
subplot(2,1,2), hold on,
   plot(ptnums_tight, abs(real(pcalf_tight)),'r-'),
   stem(ptnums_tight, abs(real(pcalf_tight)),'r'), 
   plot(ptnums_tight, abs(imag(pcalf_tight)),'g-'),
   stem(ptnums_tight, abs(imag(pcalf_tight)),'g'), 
   axis tight, grid on,
   legend('abs(real(pcal))','abs(real(pcal))','abs(imag(pcal))','abs(imag(pcal))'),
   ylabel('Linear'), xlabel('Tone number'),
   title('Compressed PCal profile with jus the tones - complex');

