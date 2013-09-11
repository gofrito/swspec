function [histo_acc]=ibobDataHist(fn)
%------------------------------------------------------------------------------------------------
% Plot histogram from data consisting of 12/16-bit samples with two or four channels
%
% The two-channel format is [ch1 samp1 | ch2 samp1 | ch1 samp2 | ch2 samp2 | ch1 samp3 ... ]
% The four-channel format is [ch1 samp1 | ch2 samp1 | ch3 samp1 | ch4 samp1 | ch1 samp2 ... ]
%------------------------------------------------------------------------------------------------

% clear;
% fn = '/raid/vemex_ibob/19042008/vex/ibob1/chQslice34.raw';

fid = fopen(fn,'rb', 'l'); % 'l' little-endian, 'b' big-endian data

N_max = 8*1024; % how large blocks to process
iter_max = 32;  % how many blocks at most

bits = 8;
histo_min = -(2^(bits-1));
histo_max = 2^(bits-1) - 1;
histo_pts = 2^bits;
histo_acc = zeros(1, histo_pts);
histo_x   = linspace(histo_min, histo_max, histo_pts);

peak_min = histo_max;
peak_max = histo_min;

iter = 1;
while ((feof(fid) == 0) && (iter <= iter_max))

   % iBOB single channel 8-bit signed data
   data = fread(fid, [1 N_max], 'schar');

   mindata = min(data);
   maxdata = max(data);
   if (mindata < peak_min), peak_min = mindata; end;
   if (maxdata > peak_max), peak_max = maxdata; end;

   [histo_tmp, desc] = histogram(data, [histo_min, histo_max, histo_pts]);
   histo_acc = histo_acc + histo_tmp;

   figure(1), 
   % stem(histo_acc), 
   plot(histo_x, histo_acc), grid on,
   title(['Accumulated histogram of ', fn]);
   fprintf(1, 'Iteration %u : overall minimum %f, maximum %f\n', iter, peak_min, peak_max);
   sleep(0);

   iter = iter + 1;

end;

fclose(fid);

histo_acc = histo_acc';
save('-text', 'values.txt', 'histo_acc');
