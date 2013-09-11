%--------------------------------------------------------------------------
% Plot data consisting of 8-bit samples with 1 channel
%
% The input format is [ch1 sample 1 | ch1 sample 2 | ch1 sample 3 | ... ]
%--------------------------------------------------------------------------

clear;

% input file
% fn='/raid/ibob-IF_C-withPCAL-750M50kTune.cap';
% fn='/raid/ibob-IF_C-withPCAL-750M25kTune.cap';
% fn='/raid/ibob-Ichannel-32Msps.cap';
fn='/raid/dump';

% sampling freq in MHz
fs = 16;

fid = fopen(fn,'rb');
fft_pts = 32000;
fft_nr  = 32;
N_max = fft_nr * fft_pts;

data  = zeros([1 fft_pts]); % raw samples
fdata = zeros([1 fft_pts]); % power spectrum

if (fft_nr>1)
  wf = hamming(fft_pts)';
  for i=0:(fft_nr-1)
    % insert 50% new data at the right, window, FFT, integrate
    halfdata  = fread(fid, [1 fft_pts/2], 'schar');
    data      = [data(fft_pts/2+1 : fft_pts) halfdata];
    windata   = data .* wf;
    fresult = fft(windata);
    fdata  = fdata + abs(fresult .* conj(fresult));
  end
else
  data    = fread(fid, [1 N_max], 'schar');
  fresult = fft(data);
  fdata   = abs(fresult); % .* conj(fresult));
end

fclose(fid);

fsize  = max(size(fdata));
ffreqs = linspace(0, fs, fsize)';
[fpeak_val, fpeak_idx] = max(fdata);

fprintf(1, "Plots...\r\n");

figure(1);

subplot(3,1,1),
% plot(ffreqs(1:fsize/2), fdata(1:fsize/2)),
semilogy(ffreqs(1:fsize/2), fdata(1:fsize/2)),
xlabel('MHz'), ylabel('Power');

subplot(3,1,2),
semilogy(ffreqs(1:fsize/6), fdata(1:fsize/6)),
xlabel('MHz'), ylabel('Power');

subplot(3,1,3),
plot(data(1:128)),
xlabel('sample nr'), ylabel('sample value');

fprintf(1, 'Assuming sampling freq %f MHz\r\n', fs);
fprintf(1, 'Peak %f at %f MHz\r\n', fpeak_val, ffreqs(fpeak_idx));

