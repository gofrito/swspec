function vsiDataPlot(fn, fs)
%--------------------------------------------------------------------------
% Plot data consisting of 12/16-bit samples with two or four channels
%
% The two-channel format is [ch1 samp1 | ch2 samp1 | ch1 samp2 | ch2 samp2 | ch1 samp3 ... ]
% The four-channel format is [ch1 samp1 | ch2 samp1 | ch3 samp1 | ch4 samp1 | ch1 samp2 ... ]
%--------------------------------------------------------------------------

fid = fopen(fn,'rb','b');
fft_pts = 8192;
fft_nr  = 64;
N_max = fft_nr * fft_pts;

% Maxim eval kit, 2 channels x 12-bit data
data = fread(fid, [1 2*N_max+1], 'int16');
data = data(2 * (1:N_max) + 0) ./ (2^4); % extract 1 out of 2 channels (0..1)

% Maxim eval kit, 4 channels x 8-bit data
% data = fread(fid, [1 4*N_max+3], 'schar');
% data = data(4 * (1:N_max) + 0); % extract 1 out of 4 channels (0..3)

fclose(fid);

if 0
  fdata = zeros([1 fft_pts]);
  for i=0:(fft_nr-1)
    dsub = data((1:fft_pts) + i*fft_pts);
    fresult = fft(dsub);
    fdata  = fdata + abs(fresult .* conj(fresult));
  end
else
  fresult = fft(data);
  fdata = abs(fresult .* conj(fresult));
end

fsize  = max(size(fdata));
ffreqs = linspace(0, fs, fsize)';
[fpeak_val, fpeak_idx] = max(fdata);

fprintf(1, 'Plots...\r\n');

figure(1);

subplot(3,1,1),
% plot(ffreqs(1:fsize/2), fdata(1:fsize/2)),
semilogy(ffreqs(1:fsize/2), fdata(1:fsize/2)),
xlabel('MHz'), ylabel('Power');

subplot(3,1,2),
semilogy(ffreqs(1:fsize/8), fdata(1:fsize/8)),
xlabel('MHz'), ylabel('Power');

subplot(3,1,3),
plot(data(1:128)),
xlabel('sample nr'), ylabel('sample value');

% gset term postscript eps color;
% gset output "/home/oper/ibob/waveform.eps";
% replot;

fprintf(1, 'Assuming sampling freq %f MHz\r\n', fs);
fprintf(1, 'Peak %f at %f MHz\r\n', fpeak_val, ffreqs(fpeak_idx));

% --- EPS export

%oneplot(); figure(3);
%plot(data(1:128));
%xlabel('sample nr'), ylabel('sample value');
%gset term postscript eps color;
%gset output "waveform.eps";
%replot;

% gunset multiplot;
%figure(3);
%semilogy(ffreqs, fdata);
%xlabel('MHz'), ylabel('r2c FFT power');
%gset term postscript eps color;
%gset output "spectrum.eps";
% gset multiplot;
%replot;
% gunset multiplot;
