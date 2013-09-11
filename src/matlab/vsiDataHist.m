%------------------------------------------------------------------------------------------------
% Plot histogram from data consisting of 12/16-bit samples with two or four channels
%
% The two-channel format is [ch1 samp1 | ch2 samp1 | ch1 samp2 | ch2 samp2 | ch1 samp3 ... ]
% The four-channel format is [ch1 samp1 | ch2 samp1 | ch3 samp1 | ch4 samp1 | ch1 samp2 ... ]
%------------------------------------------------------------------------------------------------

clear;

% fn='../rev_ex1904_Mh_No0009_histogram.raw';
fn = '../rev_ex1904_Mh_No0009_2008-04-19T07:00:00_flen=38080000000.evn';

fid = fopen(fn,'rb', 'l'); % 'l' little-endian, 'b' big-endian data

channel = 2;    % plot channel 1, 2, 3 or 4?
N_max = 8*1024; % how large blocks to process
iter_max = 16;  % how many blocks at most

histo_min = -32768;
histo_max = +32767;
histo_pts = 1024;
histo_acc = zeros(1, histo_pts);
histo_x   = linspace(histo_min, histo_max, histo_pts);

peak_min = histo_max;
peak_max = histo_min;

iter = 1;
while ((feof(fid) == 0) && (iter <= iter_max))

   % Maxim eval kit, 2 channels x 12-bit data
   rawdata = fread(fid, [1 2*N_max], 'int16');
   data = rawdata(2 * (0:(N_max-1)) + channel); % extract 1 out of 2 channels, lower 4 bits unused

   % Maxim eval kit, 4 channels x 8-bit data
   % data = fread(fid, [1 4*N_max], 'schar');
   % data = data(4 * (0:(N_max-1)) + channel); % extract 1 out of 4 channels

   mindata = min(data);
   maxdata = max(data);
   if (mindata < peak_min), peak_min = mindata; end;
   if (maxdata > peak_max), peak_max = maxdata; end;

   [histo_tmp, desc] = histogram(data, [histo_min, histo_max, histo_pts]);
   histo_acc = histo_acc + histo_tmp;

   figure(1), 
   % stem(histo_acc), 
   plot(histo_x, histo_acc), grid on,
   title(['Accumulated histogram of ', fn, ' channel ', '0'+channel]);
   fprintf(1, 'Iteration %u : overall minimum %f, maximum %f\n', iter, peak_min, peak_max);
   sleep(0);

   iter = iter + 1;

end;

fclose(fid);

histo_acc = histo_acc';
save('-text', 'values.txt', 'histo_acc');

