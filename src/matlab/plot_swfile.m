% plot_swfile(fn, Lfft, idx, fs)
%    Reads and plots one spectrum from a binary SWSpectrometer output file
%  Inputs:
%    fn   = input file and path
%    Lfft = length of FFT or DFT used in SWSpectrometer .ini file
%    idx  = index of the spectrum (1..N)
%  Optional:
%    fs = original sampling frequency in Hertz (default: 16e6)
%
function plot_swfile(fn, Lfft, idx, fs)

    % Args
    if(idx<1), idx = 1; end
    if (nargin<4),
       fs = 16e6;
    end

    % Width of data to read and plot
    Lssb = floor(Lfft/2) + 1;
    Lfloat = 4; % sizeof(float) = 4 byte

    % Read all specified spectrum
    fd = fopen(fn, 'rb', 'l');
    foff = (idx-1)*Lssb*Lfloat;
    if (fseek(fd, foff, 'bof') < 0),
       fprintf(1, 'File ended before spectrum %u\n', idx);
       return;
    end
    ydata = fread(fd, [Lssb 1], 'float32');
    fclose(fd);

    xdata = fs * (((1:Lssb)-1)/Lfft);

    if (max(size(ydata)) < Lssb),
       fprintf(1, 'Not enough data for spectrum %u (read %u, expected %u)\n', ...
               idx, max(size(ydata)), Lssb);
       return;
    end 

    % Normalize data
    ndata = (ydata - min(ydata));
    ndata = ndata / max(ydata);

    figure(1), clf;
    semilogy(xdata, ydata), axis tight;
    xlabel('freq [Hz]'), ylabel('amplitude'), title('Time-integrated DFT() data');

    fprintf(1, 'Abs Statistics: max=%f, min=%f, mean=%f, std=%f\n', max(ydata), min(ydata), mean(ydata), std(ydata));
    fprintf(1, 'Rel Statistics: mean=%f, std=%f\n', mean(ndata), std(ndata));

end
